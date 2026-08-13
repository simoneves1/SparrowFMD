// HTTP + WebSocket server for main-esp's standalone web UI, per
// main-esp/src/README.md's "web-ui" design note: serves the local UI
// and exposes the WebSocket API (web_api.h) that both the browser UI
// and (later) a farm server use.
//
// *** UNVERIFIED AGAINST REAL HARDWARE ***
// Same caveat as klipper-host-protocol's khp_uart_transport and
// storage's storage_sd: this cross-compiles for both esp32p4 (main-esp's
// actual target) and esp32s3 (a portability check, see
// main-esp/README.md), and that's the only thing that's actually been
// checked. Nothing here has run against a real network stack, a real
// browser, or a real WebSocket client. Treat it as a skeleton to build
// on and test against real hardware, not as verified working code.
//
// Static file serving is done: "/" serves webapp/index.html, embedded
// directly into the firmware image via this component's EMBED_TXTFILES
// (see CMakeLists.txt) -- no SPIFFS/SD-card dependency to view the UI.
// It's a single self-contained HTML/CSS/JS file (no build step, no
// framework) that speaks web_api.h's JSON contract directly over "/ws".
// Same unverified-against-real-hardware caveat as the rest of this file
// applies to it too -- no real browser has loaded it from a real device
// yet.
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "web_api.h"

// Registered handlers are only invoked as parsed, valid
// web_control_command values -- malformed inbound messages are
// dropped (logged, not delivered) rather than passed through half-decoded.
typedef void (*web_command_cb)(const struct web_control_command *cmd
                               , void *ctx);

struct web_server_config {
    uint16_t port;
    web_command_cb on_command; // may be NULL to ignore inbound commands
    void *cb_ctx;
};

// Starts the HTTP server and registers the "/ws" WebSocket endpoint.
// Returns NULL on failure (server start or handler registration error,
// logged via ESP_LOG).
httpd_handle_t web_server_start(const struct web_server_config *cfg);
void web_server_stop(httpd_handle_t server);

// Encodes s as JSON and sends it to every currently-connected WebSocket
// client. Failures sending to an individual client are logged and
// otherwise ignored -- one slow/dead client shouldn't block status
// updates to the rest.
void web_server_broadcast_status(httpd_handle_t server
                                 , const struct web_status *s);

#endif // web_server.h
