// See khp_usb_cdc_transport.h -- unverified against real hardware.
#include "khp_usb_cdc_transport.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"

#define KHP_USB_CDC_LIB_TASK_STACK_SIZE 4096
#define KHP_USB_CDC_LIB_TASK_PRIORITY 5
#define KHP_USB_CDC_DRIVER_TASK_STACK_SIZE 4096
#define KHP_USB_CDC_DRIVER_TASK_PRIORITY 5
#define KHP_USB_CDC_TX_TIMEOUT_MS 1000
#define KHP_USB_CDC_BULK_BUFFER_SIZE 512

struct khp_usb_cdc_ctx {
    cdc_acm_dev_hdl_t dev;
    StreamBufferHandle_t rx_stream;
    volatile bool disconnected;
};

// One physical USB link is all main-esp needs per third-party board port,
// so a single static context is enough -- khp_transport's ctx pointer
// just references it, the same way khp_uart_transport casts its
// uart_port_t directly into ctx.
static struct khp_usb_cdc_ctx s_ctx;

static void
usb_lib_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            usb_host_device_free_all();
    }
}

static bool
usb_cdc_data_rx_cb(const uint8_t *data, size_t data_len, void *user_arg)
{
    struct khp_usb_cdc_ctx *ctx = user_arg;
    // Best-effort: if the stream buffer is full, what gets dropped is
    // whichever bytes don't fit -- khp_session/khp_msgblock resyncs on
    // the next message boundary via its own garbage-discard logic, this
    // callback just must not block the class driver's task.
    xStreamBufferSend(ctx->rx_stream, data, data_len, 0);
    return true; // data consumed -- driver can reuse its RX buffer
}

static void
usb_cdc_dev_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    struct khp_usb_cdc_ctx *ctx = user_ctx;
    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED)
        ctx->disconnected = true;
}

static int
usb_cdc_transport_write(void *ctx, const uint8_t *data, size_t len)
{
    struct khp_usb_cdc_ctx *c = ctx;
    if (c->disconnected)
        return -1;
    esp_err_t err = cdc_acm_host_data_tx_blocking(c->dev, (uint8_t *)data
                                                  , len, KHP_USB_CDC_TX_TIMEOUT_MS);
    return err == ESP_OK ? (int)len : -1;
}

static int
usb_cdc_transport_read(void *ctx, uint8_t *buf, size_t buf_len, int timeout_ms)
{
    struct khp_usb_cdc_ctx *c = ctx;
    if (c->disconnected)
        return -1;
    return (int)xStreamBufferReceive(c->rx_stream, buf, buf_len
                                     , pdMS_TO_TICKS(timeout_ms));
}

bool
khp_usb_cdc_transport_init(struct khp_transport *out
                           , const struct khp_usb_cdc_transport_config *cfg)
{
    s_ctx.disconnected = false;
    s_ctx.rx_stream = xStreamBufferCreate(cfg->rx_buffer_size, 1);
    if (s_ctx.rx_stream == NULL)
        return false;

    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .enum_filter_cb = NULL,
    };
    if (usb_host_install(&host_config) != ESP_OK)
        return false;

    if (xTaskCreate(usb_lib_task, "usb_lib", KHP_USB_CDC_LIB_TASK_STACK_SIZE
                    , NULL, KHP_USB_CDC_LIB_TASK_PRIORITY, NULL) != pdPASS)
        return false;

    cdc_acm_host_driver_config_t driver_config = {
        .driver_task_stack_size = KHP_USB_CDC_DRIVER_TASK_STACK_SIZE,
        .driver_task_priority = KHP_USB_CDC_DRIVER_TASK_PRIORITY,
        .xCoreID = 0,
        .new_dev_cb = NULL,
    };
    if (cdc_acm_host_install(&driver_config) != ESP_OK)
        return false;

    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = cfg->connection_timeout_ms,
        .out_buffer_size = KHP_USB_CDC_BULK_BUFFER_SIZE,
        .in_buffer_size = KHP_USB_CDC_BULK_BUFFER_SIZE,
        .event_cb = usb_cdc_dev_event_cb,
        .data_cb = usb_cdc_data_rx_cb,
        .user_arg = &s_ctx,
    };
    if (cdc_acm_host_open(cfg->vid, cfg->pid, cfg->interface_idx
                          , &dev_config, &s_ctx.dev) != ESP_OK)
        return false;

    out->ctx = &s_ctx;
    out->write = usb_cdc_transport_write;
    out->read = usb_cdc_transport_read;
    return true;
}
