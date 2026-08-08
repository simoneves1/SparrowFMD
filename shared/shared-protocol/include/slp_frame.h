// Frame envelope for SparrowFDM's own internal UART link (main-esp <->
// touch-ui, main-esp <-> ams-esp) -- NOT related to klipper-host-
// protocol, which talks to third-party Klipper MCU boards over a
// different physical link with Klipper's own wire format. This is our
// own protocol, so unlike klipper-host-protocol there's no external
// wire-compatibility requirement to match; the framing shape here
// (length-prefixed, CRC-checked, sync-byte-terminated, resync on
// garbage) follows the same well-proven pattern for the same reasons,
// but is a fresh, independent implementation.
//
// Wire format:
//   <1 byte length><1 byte protocol_version><1 byte msg_type>
//   <n-byte payload><2 byte crc16><1 byte sync=0x7e>
// - length: total frame size (header+payload+trailer), inclusive.
// - protocol_version: must equal SLP_PROTOCOL_VERSION on both ends --
//   see the module README: mismatched firmware on independently-
//   flashed boards should fail loudly, not silently misinterpret
//   fields. This byte is what a mismatch is detected from.
// - crc16: over everything before it (length/version/type/payload).
// - sync: always 0x7e; resync-on-garbage works the same way as
//   klipper-host-protocol's msgblock (see slp_frame_check's own doc
//   comment) -- discard bytes up through the next sync byte found.
#ifndef SLP_FRAME_H
#define SLP_FRAME_H

#include <stdint.h>
#include <stddef.h>

#define SLP_PROTOCOL_VERSION 1

#define SLP_FRAME_MIN 6   // 1(len) + 1(ver) + 1(type) + 0(payload) + 2(crc) + 1(sync)
#define SLP_FRAME_MAX 64
#define SLP_FRAME_HEADER_SIZE 3   // length, version, type
#define SLP_FRAME_TRAILER_SIZE 3  // crc (2 bytes) + sync
#define SLP_FRAME_MAX_PAYLOAD (SLP_FRAME_MAX - SLP_FRAME_MIN)
#define SLP_FRAME_SYNC 0x7e

uint16_t slp_crc16_ccitt(const uint8_t *buf, uint8_t len);

// Builds a complete frame into out (needs SLP_FRAME_HEADER_SIZE +
// payload_len + SLP_FRAME_TRAILER_SIZE bytes of room). Always uses
// SLP_PROTOCOL_VERSION for the version field. Returns the total frame
// size, or 0 if payload_len > SLP_FRAME_MAX_PAYLOAD.
size_t slp_frame_encode(uint8_t *out, uint8_t msg_type
                        , const uint8_t *payload, size_t payload_len);

struct slp_frame_scanner {
    uint8_t need_sync; // sticky resync state, persists across calls --
                       // see slp_frame_check
};

// Same three-way return contract as klipper-host-protocol's
// khp_msgblock_check (0 = need more data, >0 = valid frame of that
// length, <0 = discard -return_value bytes and retry).
int slp_frame_check(struct slp_frame_scanner *scanner
                    , const uint8_t *buf, int buf_len);

enum slp_frame_status {
    SLP_FRAME_OK,
    SLP_FRAME_VERSION_MISMATCH, // frame's CRC/framing checked out, but
                                // protocol_version != SLP_PROTOCOL_VERSION
};

struct slp_frame_view {
    uint8_t msg_type;
    const uint8_t *payload;
    size_t payload_len;
};

// Call after slp_frame_check returns a positive length. Checks the
// protocol version and, if it matches, fills *out.
enum slp_frame_status slp_frame_view_init(struct slp_frame_view *out
                                          , const uint8_t *buf, int frame_len);

#endif // slp_frame.h
