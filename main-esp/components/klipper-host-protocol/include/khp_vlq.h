// VLQ (variable length quantity) integer encoding for Klipper's
// host<->MCU wire protocol. Each byte carries 7 bits of payload plus a
// continuation flag (0x80); values near zero encode in fewer bytes.
//
// This matches Klipper's own klippy/msgproto.py PT_uint32.encode/parse
// bit-for-bit -- required for wire compatibility with real Klipper MCU
// firmware and klippy, not a stylistic choice. See
// https://www.klipper3d.org/Protocol.html for the human-readable spec.
#ifndef KHP_VLQ_H
#define KHP_VLQ_H

#include <stdint.h>
#include <stddef.h>

// Longest possible encoding of a 32-bit value.
#define KHP_VLQ_MAX_BYTES 5

// Encode a signed 32-bit value into out (which must have room for at
// least KHP_VLQ_MAX_BYTES bytes). Returns the number of bytes written.
size_t khp_vlq_encode_int32(uint8_t *out, int32_t v);

// Encode an unsigned 32-bit value. Values above INT32_MAX are encoded
// using the same bit pattern a negative int32_t would use (matches the
// protocol's PT_uint32, which is signed-encoded but read back unsigned).
size_t khp_vlq_encode_uint32(uint8_t *out, uint32_t v);

// Decode one VLQ integer starting at *pp; advances *pp past the bytes
// consumed. Caller must ensure the buffer has at least one valid,
// complete encoding at *pp (i.e. don't call past the end of a message).
int32_t khp_vlq_decode_int32(const uint8_t **pp);
uint32_t khp_vlq_decode_uint32(const uint8_t **pp);

#endif // khp_vlq.h
