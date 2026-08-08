#include "khp_message.h"
#include "khp_vlq.h"
#include <string.h>

// Wire encoding is bit-identical for every integer parameter width and
// signedness (see khp_vlq.h) -- there's exactly one integer encode/
// decode path here regardless of whether the param is byte/uint16/
// int16/uint32/int32.
static bool
is_buffer_type(enum khp_param_type t)
{
    return t == KHP_PARAM_STRING || t == KHP_PARAM_BUFFER
        || t == KHP_PARAM_PROGMEM_BUFFER;
}

bool
khp_msg_encode(uint8_t *out, size_t out_cap, size_t *out_len, uint32_t msgid
              , const struct khp_param_list *params
              , const struct khp_value *values)
{
    uint8_t *p = out;
    uint8_t *end = out + out_cap;

    if ((size_t)(end - p) < KHP_VLQ_MAX_BYTES)
        return false;
    p += khp_vlq_encode_uint32(p, msgid);

    for (size_t i = 0; i < params->count; i++) {
        enum khp_param_type t = params->params[i].type;
        const struct khp_value *v = &values[i];

        if (is_buffer_type(t)) {
            if ((size_t)(end - p) < KHP_VLQ_MAX_BYTES)
                return false;
            p += khp_vlq_encode_uint32(p, (uint32_t)v->buf.len);
            if ((size_t)(end - p) < v->buf.len)
                return false;
            memcpy(p, v->buf.data, v->buf.len);
            p += v->buf.len;
        } else {
            if ((size_t)(end - p) < KHP_VLQ_MAX_BYTES)
                return false;
            p += khp_vlq_encode_int32(p, v->i);
        }
    }

    *out_len = (size_t)(p - out);
    return true;
}

bool
khp_msg_decode(const uint8_t *content, size_t content_len
              , uint32_t expected_msgid, const struct khp_param_list *params
              , struct khp_value *out_values)
{
    if (content_len < 1)
        return false;
    const uint8_t *p = content;
    const uint8_t *end = content + content_len;

    uint32_t msgid = khp_vlq_decode_uint32(&p);
    if (p > end || msgid != expected_msgid)
        return false;

    for (size_t i = 0; i < params->count; i++) {
        if (p >= end) // every field, including a zero-length buffer's
            return false; // length byte, needs at least 1 more byte
        enum khp_param_type t = params->params[i].type;

        if (is_buffer_type(t)) {
            uint32_t len = khp_vlq_decode_uint32(&p);
            if (p > end || len > (size_t)(end - p))
                return false;
            out_values[i].buf.data = p;
            out_values[i].buf.len = len;
            p += len;
        } else {
            out_values[i].i = khp_vlq_decode_int32(&p);
            if (p > end)
                return false;
        }
    }

    return p == end; // reject trailing bytes: a format/param-list mismatch
}
