// See slp_uart_transport.h -- unverified against real hardware.
#include "slp_uart_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SLP_UART_RX_BUF_SIZE 512
#define SLP_UART_TX_BUF_SIZE 0 // 0: transmit is blocking, no separate TX ring

static int
uart_transport_write(void *ctx, const uint8_t *data, size_t len)
{
    uart_port_t port = (uart_port_t)(intptr_t)ctx;
    return uart_write_bytes(port, (const char *)data, len);
}

static int
uart_transport_read(void *ctx, uint8_t *buf, size_t buf_len, int timeout_ms)
{
    uart_port_t port = (uart_port_t)(intptr_t)ctx;
    return uart_read_bytes(port, buf, buf_len, pdMS_TO_TICKS(timeout_ms));
}

bool
slp_uart_transport_init(struct slp_transport *out
                        , const struct slp_uart_transport_config *cfg)
{
    uart_config_t uart_cfg = {
        .baud_rate = cfg->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    if (uart_param_config(cfg->port, &uart_cfg) != ESP_OK)
        return false;
    if (uart_set_pin(cfg->port, cfg->tx_pin, cfg->rx_pin
                     , UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
        return false;
    if (uart_driver_install(cfg->port, SLP_UART_RX_BUF_SIZE
                            , SLP_UART_TX_BUF_SIZE, 0, NULL, 0) != ESP_OK)
        return false;

    out->ctx = (void *)(intptr_t)cfg->port;
    out->write = uart_transport_write;
    out->read = uart_transport_read;
    return true;
}
