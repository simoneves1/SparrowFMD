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

// Single-server-instance assumption throughout this file (main-esp only
// ever runs one web-ui server) -- matches how storage_sd.c keeps its
// one sdmmc_card_t* in a static rather than threading a context object
// through every call.
static struct web_server_config s_ctx;

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
        struct web_command_ack ack = {0};
        if (web_control_command_from_json((const char *)buf, frame.len, &cmd)) {
            struct web_server_config *hctx =
                (struct web_server_config *)req->user_ctx;
            if (hctx && hctx->on_command)
                hctx->on_command(&cmd, hctx->cb_ctx);
            // "ok" here means "reached the server and was dispatched",
            // not "the printer did it" -- see web_api.h's top comment.
            // on_command has no return value to report real success/
            // failure with yet, since no real gcode/kinematics execution
            // pipeline exists to report it from.
            ack.command = cmd.command;
            ack.ok = true;
        } else {
            ESP_LOGW(TAG, "dropped a malformed/incomplete command message");
            ack.command = WEB_CMD_UNKNOWN;
            ack.ok = false;
            strncpy(ack.message, "malformed or unrecognized command"
                   , sizeof(ack.message) - 1);
        }

        char *ack_json = web_command_ack_to_json(&ack);
        if (ack_json) {
            httpd_ws_frame_t ack_frame = {0};
            ack_frame.type = HTTPD_WS_TYPE_TEXT;
            ack_frame.payload = (uint8_t *)ack_json;
            ack_frame.len = strlen(ack_json);
            // Synchronous send back on this same request/connection --
            // unlike web_server_broadcast_status's async fan-out to every
            // client, this reply is only meaningful to whoever sent the
            // command that triggered it.
            if (httpd_ws_send_frame(req, &ack_frame) != ESP_OK)
                ESP_LOGW(TAG, "failed to send command_ack");
            web_json_free(ack_json);
        }
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t
send_json_or_500(httpd_req_t *req, char *json)
{
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR
                            , "failed to build response");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, strlen(json));
    web_json_free(json);
    return err;
}

static esp_err_t
files_handler(httpd_req_t *req)
{
    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    struct web_file_entry entries[WEB_SERVER_MAX_FILES];
    size_t count = (cfg && cfg->on_files_list)
        ? cfg->on_files_list(entries, WEB_SERVER_MAX_FILES, cfg->cb_ctx) : 0;
    return send_json_or_500(req, web_file_list_to_json(entries, count));
}

static esp_err_t
history_handler(httpd_req_t *req)
{
    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    struct web_print_history_entry entries[WEB_SERVER_MAX_HISTORY];
    size_t count = (cfg && cfg->on_history_list)
        ? cfg->on_history_list(entries, WEB_SERVER_MAX_HISTORY, cfg->cb_ctx) : 0;
    return send_json_or_500(req, web_print_history_to_json(entries, count));
}

static esp_err_t
diagnostics_handler(httpd_req_t *req)
{
    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    struct web_link_status links[WEB_SERVER_MAX_LINKS];
    size_t count = (cfg && cfg->on_diagnostics)
        ? cfg->on_diagnostics(links, WEB_SERVER_MAX_LINKS, cfg->cb_ctx) : 0;
    return send_json_or_500(req, web_link_status_list_to_json(links, count));
}

static esp_err_t
camera_handler(httpd_req_t *req)
{
    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    struct web_camera_config camera_cfg = {.mode = WEB_CAMERA_NONE};
    if (cfg && cfg->on_camera_config)
        cfg->on_camera_config(&camera_cfg, cfg->cb_ctx);
    return send_json_or_500(req, web_camera_config_to_json(&camera_cfg));
}

static esp_err_t
camera_post_handler(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len >= 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST
                            , "missing or oversized body");
        return ESP_FAIL;
    }
    char buf[256];
    int received = httpd_req_recv(req, buf, req->content_len);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "failed to read body");
        return ESP_FAIL;
    }

    struct web_camera_config camera_cfg;
    if (!web_camera_config_from_json(buf, (size_t)received, &camera_cfg)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST
                            , "malformed camera config");
        return ESP_FAIL;
    }

    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    bool ok = cfg && cfg->on_camera_config_set
        && cfg->on_camera_config_set(&camera_cfg, cfg->cb_ctx);
    if (!ok) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR
                            , "camera config save failed or not supported");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// See web_server.h's top comment: no camera capture backend (MIPI-CSI or
// USB UVC) is wired up yet, so this endpoint exists (GET /api/camera can
// point a browser at it without a 404) but honestly reports that it
// can't do anything yet, rather than hanging or faking a stream.
static esp_err_t
camera_stream_handler(httpd_req_t *req)
{
    httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED
                        , "no local camera capture backend wired up yet");
    return ESP_OK;
}

static esp_err_t
settings_get_handler(httpd_req_t *req)
{
    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    char buf[WEB_SERVER_MAX_SETTINGS_BYTES] = {0};
    if (cfg && cfg->on_settings_get)
        cfg->on_settings_get(buf, sizeof(buf), cfg->cb_ctx);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, buf, strlen(buf));
}

static esp_err_t
settings_post_handler(httpd_req_t *req)
{
    if (req->content_len >= WEB_SERVER_MAX_SETTINGS_BYTES) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "settings text too long");
        return ESP_FAIL;
    }
    char buf[WEB_SERVER_MAX_SETTINGS_BYTES];
    int received = httpd_req_recv(req, buf, req->content_len);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "failed to read body");
        return ESP_FAIL;
    }

    struct web_server_config *cfg = (struct web_server_config *)req->user_ctx;
    bool ok = cfg && cfg->on_settings_set
        && cfg->on_settings_set(buf, (size_t)received, cfg->cb_ctx);
    if (!ok) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR
                            , "settings save failed or not supported");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

httpd_handle_t
web_server_start(const struct web_server_config *cfg)
{
    s_ctx = *cfg;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = cfg->port;
    // Default (8) is two short of what this server now registers (ws,
    // index, files, history, diagnostics, camera GET+POST, camera/stream,
    // settings GET+POST = 10) -- explicit and padded rather than relying
    // on the default happening to still be enough as this grows.
    config.max_uri_handlers = 12;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return NULL;
    }

    httpd_uri_t ws_uri = {
        .uri = "/ws", .method = HTTP_GET, .handler = ws_handler
        , .user_ctx = &s_ctx, .is_websocket = true,
    };
    httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler},
        {.uri = "/api/files", .method = HTTP_GET, .handler = files_handler
         , .user_ctx = &s_ctx},
        {.uri = "/api/history", .method = HTTP_GET, .handler = history_handler
         , .user_ctx = &s_ctx},
        {.uri = "/api/diagnostics", .method = HTTP_GET
         , .handler = diagnostics_handler, .user_ctx = &s_ctx},
        {.uri = "/api/camera", .method = HTTP_GET, .handler = camera_handler
         , .user_ctx = &s_ctx},
        {.uri = "/api/camera", .method = HTTP_POST
         , .handler = camera_post_handler, .user_ctx = &s_ctx},
        {.uri = "/camera/stream", .method = HTTP_GET
         , .handler = camera_stream_handler},
        {.uri = "/api/settings", .method = HTTP_GET
         , .handler = settings_get_handler, .user_ctx = &s_ctx},
        {.uri = "/api/settings", .method = HTTP_POST
         , .handler = settings_post_handler, .user_ctx = &s_ctx},
    };

    if (httpd_register_uri_handler(server, &ws_uri) != ESP_OK) {
        ESP_LOGE(TAG, "failed to register /ws handler");
        httpd_stop(server);
        return NULL;
    }
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        if (httpd_register_uri_handler(server, &handlers[i]) != ESP_OK) {
            ESP_LOGE(TAG, "failed to register %s handler (method %d)"
                    , handlers[i].uri, handlers[i].method);
            httpd_stop(server);
            return NULL;
        }
    }

    return server;
}

void
web_server_stop(httpd_handle_t server)
{
    if (server)
        httpd_stop(server);
}

// Shared by web_server_broadcast_status/web_server_broadcast_console_log
// -- takes ownership of json (frees it), same "one slow/dead client
// shouldn't block the rest" semantics either way.
static void
broadcast_json(httpd_handle_t server, char *json)
{
    if (!server || !json) {
        if (json)
            web_json_free(json);
        return;
    }

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

void
web_server_broadcast_status(httpd_handle_t server, const struct web_status *s)
{
    broadcast_json(server, web_status_to_json(s));
}

void
web_server_broadcast_console_log(httpd_handle_t server
                                 , const struct web_console_log *c)
{
    broadcast_json(server, web_console_log_to_json(c));
}
