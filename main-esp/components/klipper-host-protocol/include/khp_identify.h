// The "identify" handshake: how the host discovers what commands/
// responses/config an MCU supports. The MCU's data dictionary (a
// zlib-compressed JSON blob) is too large for one message block, so the
// host requests it in chunks: send `identify offset=<N> count=<C>`,
// get back `identify_response offset=<N> data=<bytes>`, repeat with
// offset advanced by however many bytes came back, until a response
// comes back with zero bytes of data (dictionary fully received).
//
// "identify" is command id 1 and "identify_response" is command id 0 --
// both hard-coded in the protocol (they have to be, since you need this
// handshake to complete before you know any other command's id).
// See https://www.klipper3d.org/Protocol.html and klippy/serialhdl.py's
// _start_session/identify handling.
//
// This module only covers building requests, parsing responses, and
// reassembling the chunks into one contiguous (still-compressed) byte
// buffer. Decompressing that buffer (zlib inflate) and parsing the
// resulting JSON dictionary is deliberately out of scope here -- it
// needs a real inflate implementation, which is its own decision to
// make (vendor one, or use ESP-IDF's ROM-provided miniz where
// available) rather than something to fold in silently.
#ifndef KHP_IDENTIFY_H
#define KHP_IDENTIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KHP_IDENTIFY_REQUEST_CMD_ID 1
#define KHP_IDENTIFY_RESPONSE_CMD_ID 0

// How many bytes of dictionary to ask for per request. Must be small
// enough that identify_response's encoded content (cmd_id + offset +
// VLQ-length-prefixed data) fits in KHP_MSG_MAX_CONTENT -- Klipper's
// own host uses 40, which leaves comfortable headroom in a 64-byte
// message block.
#define KHP_IDENTIFY_CHUNK_SIZE 40

// Encode an "identify offset=<offset> count=<count>" request into out
// (a message block's content buffer). Returns the content length.
size_t khp_identify_build_request(uint8_t *out, uint32_t offset, uint32_t count);

// Parse an "identify_response offset=<offset> data=<bytes>" message's
// content. *data is set to point inside `content` (no copy). Returns
// false if content doesn't decode as an identify_response (wrong
// command id or truncated) -- the caller should treat that as a
// protocol error, not just "no more data".
bool khp_identify_parse_response(const uint8_t *content, size_t content_len
                                 , uint32_t *offset, const uint8_t **data
                                 , size_t *data_len);

// Reassembles chunked identify_response data into one contiguous
// buffer, tracking the next offset to request and detecting the
// zero-length-response termination condition. Owns a heap buffer that
// grows as chunks arrive.
struct khp_identify_session {
    uint8_t *buf;
    size_t len;      // bytes received so far (== next offset to request)
    size_t cap;       // allocated size of buf
    bool complete;    // true once a zero-length chunk has been received
    bool error;       // true if a response failed to parse or was out of order
};

void khp_identify_session_init(struct khp_identify_session *s);
void khp_identify_session_free(struct khp_identify_session *s);

// Build the next identify request for this session's current offset.
// Returns 0 (and writes nothing) if the session is already complete or
// in an error state -- check those first.
size_t khp_identify_session_next_request(struct khp_identify_session *s
                                         , uint8_t *out);

// Feed one identify_response message's content into the session. On a
// well-formed, in-order response this appends the data and advances the
// session (marking it complete if the chunk was empty) and returns
// true. On a malformed message or an offset that doesn't match what
// this session expected next, sets s->error and returns false --
// Klipper's real host would treat this as a fatal handshake error, not
// something to silently retry past.
bool khp_identify_session_handle_response(struct khp_identify_session *s
                                          , const uint8_t *content
                                          , size_t content_len);

#endif // khp_identify.h
