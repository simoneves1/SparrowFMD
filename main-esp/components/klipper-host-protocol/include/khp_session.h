// Ties a khp_transport (raw byte I/O) to khp_msgblock (message framing):
// pulls bytes off the transport, scans them for complete message
// blocks (discarding garbage and resyncing exactly like
// khp_msgblock_check documents), and hands each message's content to a
// callback. Also frames and sends outgoing content.
//
// This layer is deliberately transport-agnostic and has no ESP-IDF
// dependency, so unlike khp_uart_transport it's fully unit-testable
// with a mock transport -- see test/test_main.c.
//
// Known simplification: khp_session_send just assigns the next
// sequence number and writes the block; it does not implement the
// protocol's ack/nak/retransmission/windowing scheme (see
// https://www.klipper3d.org/Protocol.html's "Message Flow and
// Reliability" section). That's real, meaningful protocol behavior
// that's still missing -- flagging it here rather than pretending this
// is a complete transport-reliability implementation.
#ifndef KHP_SESSION_H
#define KHP_SESSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "khp_transport.h"
#include "khp_msgblock.h"

// Size of the internal receive buffer. Must be at least KHP_MSG_MAX so
// a single maximally-sized message block can always be fully buffered.
#define KHP_SESSION_RXBUF_SIZE 256

struct khp_session {
    struct khp_transport transport;
    struct khp_msgblock_scanner scanner;
    uint8_t rxbuf[KHP_SESSION_RXBUF_SIZE];
    size_t rxbuf_len;
    uint8_t next_seq;
};

void khp_session_init(struct khp_session *s, struct khp_transport transport);

typedef void (*khp_message_cb)(void *ctx, const uint8_t *content
                               , size_t content_len, uint8_t seq);

// Reads whatever the transport has available (waiting up to
// timeout_ms), then extracts and dispatches every complete message
// block now sitting in the receive buffer -- there may be zero, one,
// or several per call, depending on how much data arrived and how much
// was already buffered from a previous call.
//
// Returns the number of messages dispatched (>= 0), or a negative
// value if the transport's read() reported a hard error.
int khp_session_poll(struct khp_session *s, int timeout_ms
                     , khp_message_cb cb, void *cb_ctx);

// Frames content (content_len bytes, <= KHP_MSG_MAX_CONTENT) with the
// session's next sequence number and writes it via the transport.
// Returns false if content_len is out of range or the transport's
// write() didn't accept the whole block.
bool khp_session_send(struct khp_session *s, const uint8_t *content
                      , size_t content_len);

#endif // khp_session.h
