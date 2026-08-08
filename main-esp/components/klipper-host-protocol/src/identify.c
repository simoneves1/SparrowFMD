#include <stdlib.h>
#include <string.h>
#include "khp_identify.h"
#include "khp_vlq.h"

size_t
khp_identify_build_request(uint8_t *out, uint32_t offset, uint32_t count)
{
    uint8_t *p = out;
    p += khp_vlq_encode_uint32(p, KHP_IDENTIFY_REQUEST_CMD_ID);
    p += khp_vlq_encode_uint32(p, offset);
    p += khp_vlq_encode_uint32(p, count);
    return (size_t)(p - out);
}

bool
khp_identify_parse_response(const uint8_t *content, size_t content_len
                            , uint32_t *offset, const uint8_t **data
                            , size_t *data_len)
{
    // Every field below takes at least one byte, and there are three
    // fixed fields before the (possibly zero-length) data payload.
    //
    // Note: khp_vlq_decode_* has no internal bounds checking (matching
    // Klipper's own reference decoder), so a truncated/malformed field
    // can read a few bytes past `content + content_len` before the
    // `p > end` checks below catch it and reject the message. That's
    // safe in practice here because `content` always points inside a
    // fixed-size message-block buffer (at most KHP_MSG_MAX bytes) and
    // the over-read can't walk past that buffer's own trailer bytes --
    // but it does mean this function isn't safe to call against a
    // `content` pointer near the end of an arbitrary/untrusted buffer.
    if (content_len < 3)
        return false;

    const uint8_t *p = content;
    const uint8_t *end = content + content_len;

    uint32_t cmd_id = khp_vlq_decode_uint32(&p);
    if (p > end || cmd_id != KHP_IDENTIFY_RESPONSE_CMD_ID)
        return false;

    uint32_t off = khp_vlq_decode_uint32(&p);
    if (p > end)
        return false;

    uint32_t len = khp_vlq_decode_uint32(&p);
    if (p > end || len > (size_t)(end - p))
        return false;

    *offset = off;
    *data = p;
    *data_len = len;
    return true;
}

void
khp_identify_session_init(struct khp_identify_session *s)
{
    memset(s, 0, sizeof(*s));
}

void
khp_identify_session_free(struct khp_identify_session *s)
{
    free(s->buf);
    memset(s, 0, sizeof(*s));
}

size_t
khp_identify_session_next_request(struct khp_identify_session *s
                                  , uint8_t *out)
{
    if (s->complete || s->error)
        return 0;
    return khp_identify_build_request(out, (uint32_t)s->len
                                      , KHP_IDENTIFY_CHUNK_SIZE);
}

bool
khp_identify_session_handle_response(struct khp_identify_session *s
                                     , const uint8_t *content
                                     , size_t content_len)
{
    if (s->complete || s->error)
        return false;

    uint32_t offset;
    const uint8_t *data;
    size_t data_len;
    if (!khp_identify_parse_response(content, content_len, &offset, &data
                                     , &data_len)) {
        s->error = true;
        return false;
    }
    if (offset != s->len) {
        // Not the chunk we asked for -- a stale retransmit or a genuine
        // protocol desync. Klipper's own host treats this as fatal
        // rather than trying to reorder/dedupe, and so do we.
        s->error = true;
        return false;
    }

    if (data_len == 0) {
        s->complete = true;
        return true;
    }

    if (s->len + data_len > s->cap) {
        size_t new_cap = s->cap ? s->cap * 2 : 256;
        while (new_cap < s->len + data_len)
            new_cap *= 2;
        uint8_t *new_buf = realloc(s->buf, new_cap);
        if (!new_buf) {
            s->error = true;
            return false;
        }
        s->buf = new_buf;
        s->cap = new_cap;
    }
    memcpy(s->buf + s->len, data, data_len);
    s->len += data_len;
    return true;
}
