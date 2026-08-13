// See web_server.h -- unverified against real hardware.
#include <string.h>
#include <stdlib.h>
#include "web_server.h"
#include "esp_log.h"

static const char *TAG = "web_server";

// webapp/index.html, embedded into the firmware image by the component's
// EMBED_TXTFILES (see CMakeLists.txt) -- served as-is for "/" below, no
// SPIFFS/SD-card dependency to view the UI. EMBED_TXTFILES null-terminates
// the embedded data, so _end - _start - 1 is the string length (excluding
// that trailing NUL).
extern const uint8_t webapp_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t webapp_index_html_end[] asm("_binary_index_html_end");

static esp_err_t
index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    size_t len = (size_t)(webapp_index_html_end - webapp_index_html_start) - 1;
    return httpd_resp_send(req, (const char *)webapp_index_html_start, len);
}

// Matches HTTPD_DEFAULT_CONFIG()'s max_open_sockets (7); rounded up by
// one for headroom. If web_server_start() is ever changed to configure
// a larger max_open_sockets, this needs to grow to match.
#define WEB_SERVER_MAX_CLIENTS 8

struct ws_handler_ctx {
    web_command_cb on_command;
    void *cb_ctx;
};

// Single-server-instance assumption throughout this file (main-esp only
// ever runs one web-ui server) -- matches how storage_sd.c keeps its
// one sdmmc_card_t* in a static rather than threading a context object
// through every call.
static struct ws_handler_ctx s_ctx;

static esp_err_t
ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
        return ESP_OK; // the WebSocket handshake itself; nothing to do

    httpd_ws_frame_t frame = {0};
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0); // query frame.len
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ws recv (length probe) failed: %s", esp_err_to_name(err));
        return err;
    }
    if (frame.len == 0)
        return ESP_OK; // e.g. a control frame with no payload

    uint8_t *buf = malloc(frame.len + 1);
    if (!buf)
        return ESP_ERR_NO_MEM;
    frame.payload = buf;
    err = httpd_ws_recv_frame(req, &frame, frame.len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ws recv (payload) failed: %s", esp_err_to_name(err));
        free(buf);
        return err;
    }
    buf[frame.len] = '\0';

    if (frame.type == HTTPD_WS_TYPE_TEXT
        && web_msg_type_of((const char *)buf, frame.len) == WEB_MSG_COMMAND) {
        struct web_control_command cmd;
        if (web_control_command_from_json((const char *)buf, frame.len, &cmd)) {
            struct ws_handler_ctx *hctx = (struct ws_handler_ctx *)req->user_ctx;
            if (hctx && hctx->on_command)
                hctx->on_command(&cmd, hctx->cb_ctx);
        } else {
            ESP_LOGW(TAG, "dropped a malformed/incomplete command message");
        }
    }

    free(buf);
    return ESP_OK;
}

httpd_handle_t
web_server_start(const struct web_server_config *cfg)
{
    s_ctx.on_command = cfg->on_command;
    s_ctx.cb_ctx = cfg->cb_ctx;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = cfg->port;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return NULL;
    }

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = &s_ctx,
        .is_websocket = true,
    };
    if (httpd_register_uri_handler(server, &ws_uri) != ESP_OK) {
        ESP_LOGE(TAG, "failed to register /ws handler");
        httpd_stop(server);
        return NULL;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
    };
    if (httpd_register_uri_handler(server, &index_uri) != ESP_OK) {
        ESP_LOGE(TAG, "failed to register / handler");
        httpd_stop(server);
        return NULL;
    }

    return server;
}

void
web_server_stop(httpd_handle_t server)
{
    if (server)
        httpd_stop(server);
}

void
web_server_broadcast_status(httpd_handle_t server, const struct web_status *s)
{
    if (!server)
        return;
    char *json = web_status_to_json(s);
    if (!json)
        return;

    size_t num_clients = WEB_SERVER_MAX_CLIENTS;
    int client_fds[WEB_SERVER_MAX_CLIENTS];
    if (httpd_get_client_list(server, &num_clients, client_fds) != ESP_OK) {
        web_json_free(json);
        return;
    }

    httpd_ws_frame_t frame = {0};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t *)json;
    frame.len = strlen(json);

    for (size_t i = 0; i < num_clients; i++) {
        int fd = client_fds[i];
        if (httpd_ws_get_fd_info(server, fd) != HTTPD_WS_CLIENT_WEBSOCKET)
            continue;
        esp_err_t err = httpd_ws_send_frame_async(server, fd, &frame);
        if (err != ESP_OK)
            ESP_LOGW(TAG, "broadcast to fd %d failed: %s", fd
                    , esp_err_to_name(err));
    }

    web_json_free(json);
}
