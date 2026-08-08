#include "khp_vlq.h"

// Bit-for-bit port of klippy/msgproto.py's PT_uint32.encode(). Relies on
// >> being an arithmetic (sign-preserving) shift for negative int32_t,
// which is implementation-defined by the C standard but universal on
// every real-world target this project cares about (matches the
// assumption Klipper's own C code makes elsewhere).
size_t
khp_vlq_encode_int32(uint8_t *out, int32_t v)
{
    uint8_t *p = out;
    if (v >= 0xc000000 || v < -0x4000000)
        *p++ = (uint8_t)(((v >> 28) & 0x7f) | 0x80);
    if (v >= 0x180000 || v < -0x80000)
        *p++ = (uint8_t)(((v >> 21) & 0x7f) | 0x80);
    if (v >= 0x3000 || v < -0x1000)
        *p++ = (uint8_t)(((v >> 14) & 0x7f) | 0x80);
    if (v >= 0x60 || v < -0x20)
        *p++ = (uint8_t)(((v >> 7) & 0x7f) | 0x80);
    *p++ = (uint8_t)(v & 0x7f);
    return (size_t)(p - out);
}

size_t
khp_vlq_encode_uint32(uint8_t *out, uint32_t v)
{
    // Same bit pattern as the signed encoding -- the wire format doesn't
    // distinguish signed/unsigned, only the reader's interpretation does.
    return khp_vlq_encode_int32(out, (int32_t)v);
}

// Bit-for-bit port of klippy/msgproto.py's PT_uint32.parse().
int32_t
khp_vlq_decode_int32(const uint8_t **pp)
{
    const uint8_t *p = *pp;
    uint8_t c = *p++;
    int32_t v = c & 0x7f;
    if ((c & 0x60) == 0x60)
        v |= -0x20; // sign-extend if this turns out to be the final byte
    while (c & 0x80) {
        c = *p++;
        v = (v << 7) | (c & 0x7f);
    }
    *pp = p;
    return v;
}

uint32_t
khp_vlq_decode_uint32(const uint8_t **pp)
{
    return (uint32_t)khp_vlq_decode_int32(pp);
}
