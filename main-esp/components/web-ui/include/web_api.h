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
//    "progress":42,"elapsed_s":3723}
//   {"type":"command","command":"jog","jog_dx":-10.5,"jog_dy":5.0,
//    "jog_dz":0.0,"jog_feedrate":3000.0}
//   (jog_* fields are only meaningful/present when command == "jog")
#ifndef WEB_API_H
#define WEB_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum web_msg_type {
    WEB_MSG_STATUS,
    WEB_MSG_COMMAND,
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

struct web_status {
    enum web_print_state state;
    double hotend_temp;
    double hotend_target;
    double bed_temp;
    double bed_target;
    int progress_percent;
    uint32_t elapsed_s;
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
    WEB_CMD_UNKNOWN,
};

struct web_control_command {
    enum web_control_cmd command;
    double jog_dx;
    double jog_dy;
    double jog_dz;
    double jog_feedrate;
};

char *web_control_command_to_json(const struct web_control_command *c);
bool web_control_command_from_json(const char *json, size_t len
                                   , struct web_control_command *out);

// Frees a string returned by web_status_to_json/web_control_command_to_json.
void web_json_free(char *s);

#endif // web_api.h
