// Klipper host<->MCU message block framing:
//   <1 byte length><1 byte sequence><n-byte content><2 byte crc><1 byte sync>
// - length: total block size (header+content+trailer), inclusive.
// - sequence: low 4 bits = sequence number, high bits fixed at 0x10.
// - crc: 16-bit CCITT CRC over the header+content (not the trailer).
// - sync: always 0x7e.
// See https://www.klipper3d.org/Protocol.html for the full spec.
#ifndef KHP_MSGBLOCK_H
#define KHP_MSGBLOCK_H

#include <stdint.h>
#include <stddef.h>

#define KHP_MSG_MIN 5   // empty content: 2 header + 0 content + 3 trailer
#define KHP_MSG_MAX 64
#define KHP_MSG_HEADER_SIZE 2
#define KHP_MSG_TRAILER_SIZE 3
#define KHP_MSG_MAX_CONTENT (KHP_MSG_MAX - KHP_MSG_MIN)
#define KHP_MSG_SYNC 0x7e
#define KHP_MSG_DEST 0x10   // fixed high bits of the sequence byte
#define KHP_MSG_SEQ_MASK 0x0f

// Klipper's CRC16-CCITT variant. Bit-for-bit match to
// klippy/chelper/msgblock.c's msgblock_crc16_ccitt() -- required for
// wire compatibility with real Klipper MCU firmware and klippy, which
// both compute this exact value over the same bytes.
uint16_t khp_crc16_ccitt(const uint8_t *buf, uint8_t len);

// Build a complete message block into out (must have room for
// KHP_MSG_HEADER_SIZE + content_len + KHP_MSG_TRAILER_SIZE bytes).
// content_len must be <= KHP_MSG_MAX_CONTENT. Returns the total block
// size written, or 0 if content_len is out of range.
size_t khp_msgblock_encode(uint8_t *out, const uint8_t *content
                           , size_t content_len, uint8_t seq);

// Persisted across calls to khp_msgblock_check() on the same byte
// stream -- mirrors klippy/chelper/msgblock.c's msgblock_check()'s
// "need_sync" sticky flag exactly (see khp_msgblock_check's doc comment
// for why this can't just be recomputed fresh each call).
struct khp_msgblock_scanner {
    uint8_t need_sync;
};

// Scan buf (buf_len bytes available) for a valid message block at
// buf[0]. This is a direct, semantics-preserving port of Klipper's own
// msgblock_check() -- same three-way return contract, same resync
// behavior on garbage bytes -- so it stays interoperable with real
// Klipper MCU firmware, which implements the identical error-recovery
// state machine on data it receives from us.
//
// Returns:
//   0   need more data (buf_len is too short to decide either way yet)
//   >0  a valid message block was found; the return value is its total
//       length in bytes (matches KHP_MSG_HEADER_SIZE + content_len +
//       KHP_MSG_TRAILER_SIZE)
//   <0  no valid block at buf[0]; the caller should discard
//       -return_value bytes from the front of buf and call again
//       (this may consume a partial/false sync byte -- that's expected
//       and matches the reference resync algorithm)
int khp_msgblock_check(struct khp_msgblock_scanner *scanner
                       , const uint8_t *buf, int buf_len);

// Convenience wrapper around khp_msgblock_check() for the success case:
// once it returns a positive length, call this to get direct pointers
// to the content and sequence number without re-deriving the offsets.
struct khp_msgblock_view {
    const uint8_t *content;
    size_t content_len;
    uint8_t seq;
};
void khp_msgblock_view_init(struct khp_msgblock_view *out
                            , const uint8_t *buf, int block_len);

#endif // khp_msgblock.h
