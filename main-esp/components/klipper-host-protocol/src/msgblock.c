#include <string.h>
#include "khp_msgblock.h"

// Bit-for-bit port of klippy/chelper/msgblock.c's
// msgblock_crc16_ccitt() -- not a textbook CRC16/CCITT formulation, but
// this exact bit-shuffled variant is what real Klipper MCU firmware and
// klippy both compute, so it has to match precisely for the CRC to ever
// agree with a real device.
uint16_t
khp_crc16_ccitt(const uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xffff;
    while (len--) {
        uint8_t data = *buf++;
        data ^= crc & 0xff;
        data ^= (uint8_t)(data << 4);
        crc = (uint16_t)((((uint16_t)data << 8) | (crc >> 8))
                          ^ (uint8_t)(data >> 4)
                          ^ ((uint16_t)data << 3));
    }
    return crc;
}

size_t
khp_msgblock_encode(uint8_t *out, const uint8_t *content
                    , size_t content_len, uint8_t seq)
{
    if (content_len > KHP_MSG_MAX_CONTENT)
        return 0;
    size_t block_len = KHP_MSG_HEADER_SIZE + content_len + KHP_MSG_TRAILER_SIZE;
    out[0] = (uint8_t)block_len;
    out[1] = (uint8_t)(KHP_MSG_DEST | (seq & KHP_MSG_SEQ_MASK));
    memcpy(out + KHP_MSG_HEADER_SIZE, content, content_len);
    uint16_t crc = khp_crc16_ccitt(out, (uint8_t)(block_len - KHP_MSG_TRAILER_SIZE));
    out[block_len - 3] = (uint8_t)(crc >> 8);
    out[block_len - 2] = (uint8_t)crc;
    out[block_len - 1] = KHP_MSG_SYNC;
    return block_len;
}

// Direct port of klippy/chelper/msgblock.c's msgblock_check(). See that
// function (and khp_msgblock_check's own doc comment) for why the
// control flow and "need_sync" handling are structured this way rather
// than simplified -- it needs to match the reference resync behavior
// bug-for-bug, not just algorithm-for-algorithm.
int
khp_msgblock_check(struct khp_msgblock_scanner *scanner
                   , const uint8_t *buf, int buf_len)
{
    if (buf_len < KHP_MSG_MIN)
        return 0;
    if (scanner->need_sync)
        goto error;
    uint8_t msglen = buf[0];
    if (msglen < KHP_MSG_MIN || msglen > KHP_MSG_MAX)
        goto error;
    uint8_t msgseq = buf[1];
    if ((msgseq & ~KHP_MSG_SEQ_MASK) != KHP_MSG_DEST)
        goto error;
    if (buf_len < msglen)
        return 0;
    if (buf[msglen - 1] != KHP_MSG_SYNC)
        goto error;
    uint16_t msgcrc = (uint16_t)((buf[msglen - 3] << 8) | buf[msglen - 2]);
    uint16_t crc = khp_crc16_ccitt(buf, (uint8_t)(msglen - KHP_MSG_TRAILER_SIZE));
    if (crc != msgcrc)
        goto error;
    return msglen;

error: ;
    const uint8_t *next_sync = memchr(buf, KHP_MSG_SYNC, (size_t)buf_len);
    if (next_sync) {
        scanner->need_sync = 0;
        return -(int)(next_sync - buf + 1);
    }
    scanner->need_sync = 1;
    return -buf_len;
}

void
khp_msgblock_view_init(struct khp_msgblock_view *out
                       , const uint8_t *buf, int block_len)
{
    out->content = buf + KHP_MSG_HEADER_SIZE;
    out->content_len = (size_t)block_len - KHP_MSG_HEADER_SIZE - KHP_MSG_TRAILER_SIZE;
    out->seq = buf[1] & KHP_MSG_SEQ_MASK;
}
