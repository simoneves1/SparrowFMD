#include <string.h>
#include "slp_frame.h"

// Standard CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff, no reflection,
// no output xor). No wire-compatibility requirement to match anyone
// else's implementation here (unlike klipper-host-protocol's CRC),
// so this is the plain textbook bit-at-a-time formulation.
uint16_t
slp_crc16_ccitt(const uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xffff;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t
slp_frame_encode(uint8_t *out, uint8_t msg_type, const uint8_t *payload
                 , size_t payload_len)
{
    if (payload_len > SLP_FRAME_MAX_PAYLOAD)
        return 0;

    size_t frame_len = SLP_FRAME_HEADER_SIZE + payload_len
                      + SLP_FRAME_TRAILER_SIZE;
    out[0] = (uint8_t)frame_len;
    out[1] = SLP_PROTOCOL_VERSION;
    out[2] = msg_type;
    if (payload_len)
        memcpy(out + SLP_FRAME_HEADER_SIZE, payload, payload_len);

    uint16_t crc = slp_crc16_ccitt(out
        , (uint8_t)(frame_len - SLP_FRAME_TRAILER_SIZE));
    out[frame_len - 3] = (uint8_t)(crc >> 8);
    out[frame_len - 2] = (uint8_t)crc;
    out[frame_len - 1] = SLP_FRAME_SYNC;
    return frame_len;
}

int
slp_frame_check(struct slp_frame_scanner *scanner, const uint8_t *buf
                , int buf_len)
{
    if (buf_len < SLP_FRAME_MIN)
        return 0;
    if (scanner->need_sync)
        goto error;

    uint8_t frame_len = buf[0];
    if (frame_len < SLP_FRAME_MIN || frame_len > SLP_FRAME_MAX)
        goto error;
    if (buf_len < frame_len)
        return 0;
    if (buf[frame_len - 1] != SLP_FRAME_SYNC)
        goto error;

    uint16_t frame_crc = (uint16_t)((buf[frame_len - 3] << 8)
                                    | buf[frame_len - 2]);
    uint16_t crc = slp_crc16_ccitt(buf
        , (uint8_t)(frame_len - SLP_FRAME_TRAILER_SIZE));
    if (crc != frame_crc)
        goto error;

    return frame_len;

error: ;
    const uint8_t *next_sync = memchr(buf, SLP_FRAME_SYNC, (size_t)buf_len);
    if (next_sync) {
        scanner->need_sync = 0;
        return -(int)(next_sync - buf + 1);
    }
    scanner->need_sync = 1;
    return -buf_len;
}

enum slp_frame_status
slp_frame_view_init(struct slp_frame_view *out, const uint8_t *buf
                    , int frame_len)
{
    if (buf[1] != SLP_PROTOCOL_VERSION)
        return SLP_FRAME_VERSION_MISMATCH;

    out->msg_type = buf[2];
    out->payload = buf + SLP_FRAME_HEADER_SIZE;
    out->payload_len = (size_t)frame_len - SLP_FRAME_HEADER_SIZE
                      - SLP_FRAME_TRAILER_SIZE;
    return SLP_FRAME_OK;
}
