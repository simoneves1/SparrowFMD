// JSON message API for main-esp's web-ui WebSocket endpoint. Per
// main-esp/src/README.md's design note, this is meant to be "the same
// API the Touch UI and (later) farm server use" -- meaning the same
// domain concepts (status: state/temps/progress; control: start/stop/
// pause/resume/home/jog) as shared-protocol's status_update/
// control_command, not literally the same bytes. Browsers and a future
// farm server naturally want JSON over a WebSocket; touch-ui and
// ams-esp get the same concepts over shared-protocol's compact binary
// frames instead. This module has no dependency on shared-protocol and
// isn't a serialization of its structs -- it's an independent JSON
// encoding of the same ideas, kept in sync by design intent, not by
// code sharing.
//
// Message shape (top-level "type" field distinguishes the two):
//   {"type":"status","state":"printing","hotend_temp":210.5,
//    "hotend_target":210.0,"bed_temp":60.0,"bed_target":60.0,
//    "progress":42,"elapsed_s":3723,"filename":"benchy.gcode",
//    "layer_current":87,"layer_total":214,"remaining_s":2280}
//   {"type":"command","command":"jog","jog_dx":-10.5,"jog_dy":5.0,
//    "jog_dz":0.0,"jog_feedrate":3000.0}
//   (jog_* fields are only meaningful/present when command == "jog";
//   jog_dz/jog_feedrate are reused for the 4 filament commands' move
//   distance/speed -- same "move an axis some distance at some feedrate"
//   shape, mirroring shared-protocol's slp_control_command choice to
//   reuse those fields rather than add duplicates)
//   {"type":"command","command":"set_hotend_temp","target_temp":210.0}
//   (target_temp is only meaningful/present for set_hotend_temp/set_bed_temp)
//   {"type":"command","command":"start","filename":"benchy.gcode"}
//   (filename is only meaningful for "start"; empty/absent means "resume
//   whatever's already loaded", preserving the original no-filename
//   behavior rather than requiring every caller to know one)
//   {"type":"command","command":"gcode","gcode":"G28 X"}
//   (gcode is only meaningful/present for "gcode" -- a single raw line,
//   for the Console page; main-esp is expected to run it through the
//   same gcode-parser path as a sliced file and report the result as a
//   console_log push, see below)
//   {"type":"command","command":"set_speed_factor","value":120.0}
//   (value is only meaningful/present for set_speed_factor/
//   set_flow_factor [both a percent, 100.0 = unscaled] and set_z_offset
//   [mm delta] -- three differently-named Tune-panel commands sharing one
//   field, same "same shape, don't duplicate fields" reasoning as
//   jog_dz/jog_feedrate and target_temp above)
//
// One more message flows server -> browser over "/ws" alongside "status",
// for the Console page's response log:
//   {"type":"console_log","line":"ok"}
//
// Two more JSON shapes exist outside the WebSocket "type" dispatch above,
// since they're plain request/response data rather than push/command
// messages -- served over regular HTTP GET endpoints (see web_server.h),
// not "/ws":
//   GET /api/files  -> {"files":[{"name":"benchy.gcode","size_bytes":
//                       6291456,"print_time_s":8100}, ...]}
//   GET/POST /api/camera -> {"mode":"url","url":"http://192.168.1.50/stream"}
//                            | {"mode":"local"} | {"mode":"none"}
//   (POST body is the same shape; url is required and non-empty for
//   mode "url", ignored/optional otherwise)
//   ("local" means main-esp streams from its own attached camera at
//   "/camera/stream" -- MIPI-CSI or USB UVC, neither has a capture
//   backend wired yet, see web_server.h; "url" means a separate IP
//   camera/ESP32-CAM elsewhere on the network, which the browser can
//   embed directly, no main-esp involvement beyond reporting the URL)
#ifndef WEB_API_H
#define WEB_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum web_msg_type {
    WEB_MSG_STATUS,
    WEB_MSG_COMMAND,
    WEB_MSG_CONSOLE_LOG,
    WEB_MSG_UNKNOWN, // not valid JSON, or missing/unrecognized "type"
};

// Sniffs a raw inbound WebSocket text message's "type" field without
// fully decoding it -- lets a dispatcher pick which *_from_json to call.
enum web_msg_type web_msg_type_of(const char *json, size_t len);

enum web_print_state {
    WEB_STATE_IDLE,
    WEB_STATE_HOMING,
    WEB_STATE_PRINTING,
    WEB_STATE_PAUSED,
    WEB_STATE_ERROR,
};

// filename must be null-terminated by whoever fills in struct web_status
// (web_status_to_json does not truncate/terminate it itself, unlike
// web_status_from_json's decode side); sized to match shared-protocol's
// slp_status_update SLP_STATUS_UPDATE_FILENAME_LEN convention (kept in
// sync by design intent, not shared code).
#define WEB_STATUS_FILENAME_LEN 24

struct web_status {
    enum web_print_state state;
    double hotend_temp;
    double hotend_target;
    double bed_temp;
    double bed_target;
    int progress_percent;
    uint32_t elapsed_s;
    char filename[WEB_STATUS_FILENAME_LEN];
    int layer_current;
    int layer_total;
    uint32_t remaining_s;
    double speed_factor; // percent, 100.0 = unscaled -- Tune panel
    double flow_factor;  // percent, 100.0 = unscaled -- Tune panel
    double z_offset;     // mm -- Tune panel
};

// Returns a malloc'd (via cJSON's allocator) null-terminated JSON
// string, or NULL on failure. Free with web_json_free().
char *web_status_to_json(const struct web_status *s);
bool web_status_from_json(const char *json, size_t len, struct web_status *out);

enum web_control_cmd {
    WEB_CMD_START,
    WEB_CMD_STOP,
    WEB_CMD_PAUSE,
    WEB_CMD_RESUME,
    WEB_CMD_HOME,
    WEB_CMD_JOG,
    WEB_CMD_SET_HOTEND_TEMP,
    WEB_CMD_SET_BED_TEMP,
    WEB_CMD_FILAMENT_RETRACT,
    WEB_CMD_FILAMENT_EXTRUDE,
    WEB_CMD_FILAMENT_LOAD,
    WEB_CMD_FILAMENT_UNLOAD,
    WEB_CMD_GCODE,
    WEB_CMD_SET_SPEED_FACTOR,
    WEB_CMD_SET_FLOW_FACTOR,
    WEB_CMD_SET_Z_OFFSET,
    WEB_CMD_UNKNOWN,
};

// Raw G-code line length cap for the "gcode" command -- generous for a
// single line (real G-code lines are almost always well under this),
// not a real line-length limit.
#define WEB_GCODE_LINE_LEN 64

// Shared with struct web_status's filename -- see WEB_STATUS_FILENAME_LEN
// and struct web_file_entry below, all kept at the same length so a name
// round-tripping status -> file list -> start-command never truncates
// differently in one place than another.
#define WEB_FILENAME_LEN WEB_STATUS_FILENAME_LEN

struct web_control_command {
    enum web_control_cmd command;
    double jog_dx;
    double jog_dy;
    double jog_dz;    // also: filament move distance (mm) for the 4
                      // FILAMENT_* commands, see this header's top comment
    double jog_feedrate; // also: filament move feedrate for those commands
    double target_temp;  // only for SET_HOTEND_TEMP/SET_BED_TEMP
    double value; // only for SET_SPEED_FACTOR/SET_FLOW_FACTOR/SET_Z_OFFSET
    char filename[WEB_FILENAME_LEN]; // only for START; "" means none given
    char gcode[WEB_GCODE_LINE_LEN]; // only for GCODE
};

char *web_control_command_to_json(const struct web_control_command *c);
bool web_control_command_from_json(const char *json, size_t len
                                   , struct web_control_command *out);

// Pushed server -> browser over "/ws" for the Console page, one line at
// a time (e.g. gcode-parser's response to a "gcode" command, or an
// unsolicited status line) -- see this header's top comment for the
// JSON shape.
#define WEB_CONSOLE_LOG_LINE_LEN 128

struct web_console_log {
    char line[WEB_CONSOLE_LOG_LINE_LEN];
};

char *web_console_log_to_json(const struct web_console_log *c);
bool web_console_log_from_json(const char *json, size_t len
                               , struct web_console_log *out);

// -- /api/files -- see this header's top comment for the JSON shape.
struct web_file_entry {
    char name[WEB_FILENAME_LEN];
    uint32_t size_bytes;
    uint32_t print_time_s; // 0 if unknown/not estimated
};

char *web_file_list_to_json(const struct web_file_entry *files, size_t count);

// -- /api/camera -- see this header's top comment for the JSON shape.
enum web_camera_mode {
    WEB_CAMERA_NONE,
    WEB_CAMERA_URL,
    WEB_CAMERA_LOCAL,
};

struct web_camera_config {
    enum web_camera_mode mode;
    char url[96]; // only meaningful for WEB_CAMERA_URL
};

char *web_camera_config_to_json(const struct web_camera_config *c);
// For POST /api/camera -- rejects an unrecognized "mode" value, and
// requires "url" to be present (and non-empty) when mode == "url", the
// same shape discipline as the rest of this header's *_from_json
// functions.
bool web_camera_config_from_json(const char *json, size_t len
                                 , struct web_camera_config *out);

// Frees a string returned by any *_to_json function in this header.
void web_json_free(char *s);

#endif // web_api.h
