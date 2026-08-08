// Message payloads carried inside slp_frame envelopes -- a first,
// concrete version of the two categories shared/README.md calls out
// ("status updates (temps, progress)" and "control commands
// (start/stop/pause, jog)"). Fields here are a reasonable starting
// point, not a final spec -- expect these to grow as touch-ui/ams-esp
// integration reveals what's actually needed.
//
// All multi-byte fields are encoded big-endian, explicitly byte-by-byte
// (not via struct packing/casts), so the wire format doesn't depend on
// a compiler's struct layout or host endianness.
#ifndef SLP_MESSAGES_H
#define SLP_MESSAGES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum slp_msg_type {
    SLP_MSG_STATUS_UPDATE = 1,
    SLP_MSG_CONTROL_COMMAND = 2,
};

enum slp_print_state {
    SLP_STATE_IDLE = 0,
    SLP_STATE_HOMING = 1,
    SLP_STATE_PRINTING = 2,
    SLP_STATE_PAUSED = 3,
    SLP_STATE_ERROR = 4,
};

// Temperatures are hundredths of a degree C (e.g. 21050 == 210.50C),
// signed to allow sensor-fault/negative-reading sentinel values without
// a separate "valid" flag.
struct slp_status_update {
    enum slp_print_state state;
    int16_t hotend_temp_c_x100;
    int16_t hotend_target_c_x100;
    int16_t bed_temp_c_x100;
    int16_t bed_target_c_x100;
    uint8_t progress_percent;   // 0-100
    uint32_t elapsed_s;
};

#define SLP_STATUS_UPDATE_WIRE_SIZE 14 // 1+2+2+2+2+1+4

size_t slp_status_update_encode(uint8_t *out
                                , const struct slp_status_update *s);
bool slp_status_update_decode(const uint8_t *payload, size_t payload_len
                              , struct slp_status_update *out);

enum slp_control_cmd {
    SLP_CMD_START = 0,
    SLP_CMD_STOP = 1,
    SLP_CMD_PAUSE = 2,
    SLP_CMD_RESUME = 3,
    SLP_CMD_HOME = 4,
    SLP_CMD_JOG = 5,
};

// jog_* fields are hundredths of a mm (matches temperature's x100
// convention for consistency); only meaningful when command == SLP_CMD_JOG.
struct slp_control_command {
    enum slp_control_cmd command;
    int16_t jog_dx_mm_x100;
    int16_t jog_dy_mm_x100;
    int16_t jog_dz_mm_x100;
    uint16_t jog_feedrate_mm_min;
};

#define SLP_CONTROL_COMMAND_WIRE_SIZE 9 // 1+2+2+2+2

size_t slp_control_command_encode(uint8_t *out
                                  , const struct slp_control_command *c);
bool slp_control_command_decode(const uint8_t *payload, size_t payload_len
                                , struct slp_control_command *out);

#endif // slp_messages.h
