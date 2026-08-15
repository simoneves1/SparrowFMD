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
//
// Three plain HTTP endpoints beyond "/" and "/ws" -- see web_api.h's top
// comment for their JSON shapes:
//   GET/POST /api/settings -- raw printer.cfg-style text, backed by
//     on_settings_get/on_settings_set below (not wired to real storage
//     anywhere yet -- that's storage_sd/cfg_parser's job, this component
//     only owns the HTTP surface).
//   GET /api/files -- backed by on_files_list below, same "not wired to
//     real storage yet" caveat.
//   GET /api/history -- backed by on_history_list below, same caveat.
//   GET /api/diagnostics -- backed by on_diagnostics below.
//   GET/POST /api/camera -- backed by on_camera_config/on_camera_config_set
//     below, same "not wired to real storage yet" caveat. When GET
//     reports {"mode":"local"}, the browser is expected to point an
//     <img>/<video> at GET /camera/stream -- registered here but not
//     implemented (responds 501): main-esp has no camera capture backend
//     wired for either the P4's MIPI-CSI interface or a USB UVC webcam
//     yet. {"mode":"url"} (a separate IP camera/ESP32-CAM) needs nothing
//     from this endpoint at all, the browser embeds that URL directly.
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "web_api.h"

// Registered handlers are only invoked as parsed, valid
// web_control_command values -- malformed inbound messages are
// dropped (logged, not delivered) rather than passed through half-decoded.
typedef void (*web_command_cb)(const struct web_control_command *cmd
                               , void *ctx);

// Fills out[] with up to max entries, returns how many were written.
// May be NULL, in which case GET /api/files reports an empty list --
// this component has no SD-card access of its own (that's storage_sd's
// job), it just needs somewhere to ask.
typedef size_t (*web_files_list_cb)(struct web_file_entry *out, size_t max
                                    , void *ctx);

// Fills out[] with up to max past-print entries, most-recent-first,
// returns how many were written. May be NULL, in which case
// GET /api/history reports an empty list. See web_api.h's top comment
// for the JSON shape and main-esp/main/main.c for how an entry actually
// gets recorded in the first place (there's no real print-completion
// pipeline yet -- see that file's own comments on what "success" can
// honestly mean today).
typedef size_t (*web_history_list_cb)(struct web_print_history_entry *out
                                      , size_t max, void *ctx);

// Fills out[] with up to max monitored-link statuses, returns how many
// were written. May be NULL, in which case GET /api/diagnostics reports
// an empty list. Each entry is expected to come from a real safety
// component link_watchdog's link_watchdog_check() result -- see
// main-esp/main/main.c for how "ok" is actually decided today.
typedef size_t (*web_diagnostics_cb)(struct web_link_status *out, size_t max
                                     , void *ctx);

// Fills *out with the current camera configuration. May be NULL, in
// which case GET /api/camera always reports {"mode":"none"}.
typedef void (*web_camera_config_cb)(struct web_camera_config *out
                                     , void *ctx);

// Called with a parsed, valid camera config POSTed to /api/camera, to
// persist it (e.g. into the same config storage on_settings_set writes
// to). Returns false to make the HTTP response report a save failure.
// May be NULL, in which case POST /api/camera always fails. Malformed
// POST bodies never reach this callback, same discipline as on_command.
typedef bool (*web_camera_config_set_cb)(const struct web_camera_config *cfg
                                         , void *ctx);

// Copies up to buf_size-1 bytes of the current raw printer.cfg-style
// config text into buf (null-terminated), returns false if there's
// nothing to report (e.g. no config loaded yet). May be NULL, in which
// case GET /api/settings returns an empty document.
typedef bool (*web_settings_get_cb)(char *buf, size_t buf_size, void *ctx);

// Receives the raw text POSTed to /api/settings for the caller to
// validate/parse/persist (e.g. via storage's cfg_parser + storage_sd).
// Returns false to make the HTTP response report a save failure. May be
// NULL, in which case POST /api/settings always fails.
typedef bool (*web_settings_set_cb)(const char *text, size_t len, void *ctx);

struct web_server_config {
    uint16_t port;
    web_command_cb on_command; // may be NULL to ignore inbound commands
    web_files_list_cb on_files_list;
    web_history_list_cb on_history_list;
    web_diagnostics_cb on_diagnostics;
    web_camera_config_cb on_camera_config;
    web_camera_config_set_cb on_camera_config_set;
    web_settings_get_cb on_settings_get;
    web_settings_set_cb on_settings_set;
    void *cb_ctx;
};

// GET /api/files/max entries this component will ever ask on_files_list
// for in one call -- a fixed stack-sized cap, not a real pagination
// limit (a real SD card's file count is expected to comfortably fit;
// revisit if that stops being true).
#define WEB_SERVER_MAX_FILES 64

// Same fixed-stack-buffer reasoning as WEB_SERVER_MAX_FILES, for
// GET /api/history.
#define WEB_SERVER_MAX_HISTORY 32

// Same fixed-stack-buffer reasoning as WEB_SERVER_MAX_FILES, for
// GET /api/diagnostics.
#define WEB_SERVER_MAX_LINKS 8

// Raw config text size cap for GET/POST /api/settings, for the same
// fixed-buffer-on-the-stack reason as WEB_SERVER_MAX_FILES.
#define WEB_SERVER_MAX_SETTINGS_BYTES 4096

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

// Same broadcast semantics as web_server_broadcast_status, for the
// Console page's response log (e.g. one call per line of output from
// running a "gcode" command).
void web_server_broadcast_console_log(httpd_handle_t server
                                      , const struct web_console_log *c);

#endif // web_server.h
