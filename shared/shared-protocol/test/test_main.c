// Host-buildable unit tests for shared-protocol.
// Build with any C compiler, no ESP-IDF required:
//   gcc -Wall -I../include -o slp_test test_main.c ../src/slp_frame.c ../src/slp_messages.c
#include <stdio.h>
#include <string.h>
#include "slp_frame.h"
#include "slp_messages.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

static void
test_crc_empty(void)
{
    CHECK("crc16 of empty buffer == 0xffff"
         , slp_crc16_ccitt((const uint8_t *)"", 0) == 0xffff);
}

static void
test_frame_roundtrip(const uint8_t *payload, size_t payload_len, uint8_t type)
{
    uint8_t buf[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(buf, type, payload, payload_len);
    char desc[80];

    snprintf(desc, sizeof(desc), "frame encode payload_len=%zu succeeds"
            , payload_len);
    CHECK(desc, frame_len == SLP_FRAME_HEADER_SIZE + payload_len
                            + SLP_FRAME_TRAILER_SIZE);

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, buf, (int)frame_len);
    snprintf(desc, sizeof(desc), "frame check payload_len=%zu accepts it"
            , payload_len);
    CHECK(desc, r == (int)frame_len);

    if (r > 0) {
        struct slp_frame_view view;
        enum slp_frame_status st = slp_frame_view_init(&view, buf, r);
        snprintf(desc, sizeof(desc), "frame view payload_len=%zu correct"
                , payload_len);
        CHECK(desc, st == SLP_FRAME_OK && view.msg_type == type
                    && view.payload_len == payload_len
                    && (payload_len == 0
                        || memcmp(view.payload, payload, payload_len) == 0));
    }
}

static void
test_frame_boundaries(void)
{
    uint8_t empty[1] = {0};
    test_frame_roundtrip(empty, 0, SLP_MSG_STATUS_UPDATE);

    uint8_t max_payload[SLP_FRAME_MAX_PAYLOAD];
    for (size_t i = 0; i < sizeof(max_payload); i++)
        max_payload[i] = (uint8_t)i;
    test_frame_roundtrip(max_payload, sizeof(max_payload), SLP_MSG_CONTROL_COMMAND);
}

static void
test_frame_rejects_corrupt_crc(void)
{
    uint8_t payload[4] = {1, 2, 3, 4};
    uint8_t buf[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(buf, SLP_MSG_STATUS_UPDATE, payload
                                        , sizeof(payload));
    buf[SLP_FRAME_HEADER_SIZE] ^= 0xff;

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, buf, (int)frame_len);
    CHECK("frame check rejects corrupted payload (bad crc)", r < 0);
}

static void
test_frame_version_mismatch(void)
{
    uint8_t payload[2] = {9, 9};
    uint8_t buf[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(buf, SLP_MSG_STATUS_UPDATE, payload
                                        , sizeof(payload));
    buf[1] = SLP_PROTOCOL_VERSION + 1; // stomp the version byte
    // Recompute the CRC so the frame is still structurally valid --
    // we're testing version detection, not CRC rejection.
    uint16_t crc = slp_crc16_ccitt(buf
        , (uint8_t)(frame_len - SLP_FRAME_TRAILER_SIZE));
    buf[frame_len - 3] = (uint8_t)(crc >> 8);
    buf[frame_len - 2] = (uint8_t)crc;

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, buf, (int)frame_len);
    CHECK("frame check accepts a structurally valid but wrong-version frame"
         , r == (int)frame_len);

    if (r > 0) {
        struct slp_frame_view view;
        enum slp_frame_status st = slp_frame_view_init(&view, buf, r);
        CHECK("frame view reports a version mismatch"
             , st == SLP_FRAME_VERSION_MISMATCH);
    }
}

static void
test_frame_resync(void)
{
    uint8_t payload[2] = {0xaa, 0xbb};
    uint8_t valid[SLP_FRAME_MAX];
    size_t valid_len = slp_frame_encode(valid, SLP_MSG_STATUS_UPDATE, payload
                                        , sizeof(payload));

    uint8_t stream[16 + SLP_FRAME_MAX];
    uint8_t garbage[3] = {0x00, 0x11, SLP_FRAME_SYNC};
    memcpy(stream, garbage, sizeof(garbage));
    memcpy(stream + sizeof(garbage), valid, valid_len);
    size_t stream_len = sizeof(garbage) + valid_len;

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, stream, (int)stream_len);
    CHECK("frame resync discards garbage up to its own sync byte"
         , r < 0 && -r == (int)sizeof(garbage));

    r = slp_frame_check(&scanner, stream + sizeof(garbage)
                        , (int)(stream_len - sizeof(garbage)));
    CHECK("frame resync: remainder after discard is the valid frame"
         , r == (int)valid_len);
}

static void
test_status_update_roundtrip(void)
{
    struct slp_status_update s = {
        .state = SLP_STATE_PRINTING,
        .hotend_temp_c_x100 = 21050,
        .hotend_target_c_x100 = 21000,
        .bed_temp_c_x100 = -50, // sensor fault sentinel, negative on purpose
        .bed_target_c_x100 = 6000,
        .progress_percent = 42,
        .elapsed_s = 3723,
    };
    uint8_t payload[SLP_STATUS_UPDATE_WIRE_SIZE];
    size_t n = slp_status_update_encode(payload, &s);
    CHECK("status_update encode produces the documented wire size"
         , n == SLP_STATUS_UPDATE_WIRE_SIZE);

    struct slp_status_update out;
    bool ok = slp_status_update_decode(payload, n, &out);
    CHECK("status_update decode roundtrips every field"
         , ok && out.state == s.state
           && out.hotend_temp_c_x100 == s.hotend_temp_c_x100
           && out.hotend_target_c_x100 == s.hotend_target_c_x100
           && out.bed_temp_c_x100 == s.bed_temp_c_x100
           && out.bed_target_c_x100 == s.bed_target_c_x100
           && out.progress_percent == s.progress_percent
           && out.elapsed_s == s.elapsed_s);

    CHECK("status_update decode rejects a truncated payload"
         , !slp_status_update_decode(payload, n - 1, &out));
}

static void
test_control_command_roundtrip(void)
{
    struct slp_control_command c = {
        .command = SLP_CMD_JOG,
        .jog_dx_mm_x100 = -1050, // -10.50mm
        .jog_dy_mm_x100 = 500,
        .jog_dz_mm_x100 = 0,
        .jog_feedrate_mm_min = 3000,
    };
    uint8_t payload[SLP_CONTROL_COMMAND_WIRE_SIZE];
    size_t n = slp_control_command_encode(payload, &c);
    CHECK("control_command encode produces the documented wire size"
         , n == SLP_CONTROL_COMMAND_WIRE_SIZE);

    struct slp_control_command out;
    bool ok = slp_control_command_decode(payload, n, &out);
    CHECK("control_command decode roundtrips every field, incl. negative jog"
         , ok && out.command == c.command
           && out.jog_dx_mm_x100 == c.jog_dx_mm_x100
           && out.jog_dy_mm_x100 == c.jog_dy_mm_x100
           && out.jog_dz_mm_x100 == c.jog_dz_mm_x100
           && out.jog_feedrate_mm_min == c.jog_feedrate_mm_min);

    CHECK("control_command decode rejects a truncated payload"
         , !slp_control_command_decode(payload, n - 1, &out));
}

static void
test_status_update_through_a_real_frame(void)
{
    // End-to-end: encode a status_update, wrap it in a frame, and
    // decode both layers back out, the way main-esp/touch-ui actually
    // would over UART.
    struct slp_status_update s = {
        .state = SLP_STATE_HOMING, .hotend_temp_c_x100 = 0
        , .hotend_target_c_x100 = 0, .bed_temp_c_x100 = 0
        , .bed_target_c_x100 = 0, .progress_percent = 0, .elapsed_s = 0,
    };
    uint8_t payload[SLP_STATUS_UPDATE_WIRE_SIZE];
    slp_status_update_encode(payload, &s);

    uint8_t frame[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(frame, SLP_MSG_STATUS_UPDATE, payload
                                        , sizeof(payload));

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, frame, (int)frame_len);
    struct slp_frame_view view;
    enum slp_frame_status fst = (r > 0)
        ? slp_frame_view_init(&view, frame, r) : SLP_FRAME_VERSION_MISMATCH;

    struct slp_status_update out;
    bool ok = r == (int)frame_len && fst == SLP_FRAME_OK
        && view.msg_type == SLP_MSG_STATUS_UPDATE
        && slp_status_update_decode(view.payload, view.payload_len, &out);
    CHECK("status_update survives a full frame encode/decode round-trip"
         , ok && out.state == SLP_STATE_HOMING);
}

int
main(void)
{
    test_crc_empty();
    test_frame_boundaries();
    test_frame_rejects_corrupt_crc();
    test_frame_version_mismatch();
    test_frame_resync();
    test_status_update_roundtrip();
    test_control_command_roundtrip();
    test_status_update_through_a_real_frame();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
