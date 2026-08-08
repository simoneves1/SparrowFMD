// Generic encode/decode of a message block's *content* (the bytes
// khp_msgblock_encode/khp_msgblock_view work with) against a
// khp_param_list from khp_msgtable_lookup_params(). This is the layer
// that turns "message X has these named, typed parameters" into actual
// wire bytes and back -- everything below assumes you've already
// identified which message you're building/reading via khp_msgtable.
//
// Every content-encoded message starts with a VLQ-encoded message id,
// per the protocol's `<VLQ command-id><VLQ param1><VLQ param2>...`
// structure -- see https://www.klipper3d.org/Protocol.html.
#ifndef KHP_MESSAGE_H
#define KHP_MESSAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "khp_msgtable.h"

// One parameter's value. Which union member is valid is determined by
// the corresponding khp_param's type: KHP_PARAM_{UINT32,INT32,UINT16,
// INT16,BYTE} use `i` (the wire encoding is bit-identical regardless of
// width/signedness, so `i` always holds the raw decoded bits -- cast to
// uint32_t yourself for the unsigned types if that's what you need);
// KHP_PARAM_{STRING,BUFFER,PROGMEM_BUFFER} use `buf`.
struct khp_value {
    union {
        int32_t i;
        struct {
            const uint8_t *data; // points into the buffer that was
            size_t len;          // encoded from, or decoded into --
        } buf;                   // see each function's ownership notes
    };
};

// Encodes msgid followed by each params->params[i] using values[i]
// (values must have params->count elements) into out. Returns false
// (writing nothing usable to *out_len) if the encoding wouldn't fit in
// out_cap bytes -- check against KHP_MSG_MAX_CONTENT before allocating
// a message block around the result.
bool khp_msg_encode(uint8_t *out, size_t out_cap, size_t *out_len
                    , uint32_t msgid, const struct khp_param_list *params
                    , const struct khp_value *values);

// Decodes content (as produced by khp_msgblock_view_init's `content`/
// `content_len`) into out_values (caller-allocated, params->count
// elements). Fails if the leading msgid doesn't match expected_msgid,
// if a buffer-type parameter claims more bytes than are present, or if
// there are leftover bytes after all parameters are consumed (a format
// mismatch -- either the wrong param list was used, or the message is
// corrupt in a way the block-level CRC didn't catch, e.g. right after
// a dictionary version change).
//
// Buffer-type out_values point directly into `content` -- no copy is
// made, so `content` must stay valid for as long as those values are
// used.
bool khp_msg_decode(const uint8_t *content, size_t content_len
                    , uint32_t expected_msgid
                    , const struct khp_param_list *params
                    , struct khp_value *out_values);

#endif // khp_message.h
