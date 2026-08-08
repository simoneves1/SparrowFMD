// Ties an slp_transport (raw byte I/O) to slp_frame (frame envelope):
// pulls bytes off the transport, scans them for complete frames
// (discarding garbage and resyncing exactly like slp_frame_check
// documents), and hands each frame's type/payload to a callback. Also
// frames and sends outgoing payloads.
//
// Transport-agnostic and has no ESP-IDF dependency, so unlike
// slp_uart_transport it's fully unit-testable with a mock transport --
// see test/test_main.c. Structurally identical to
// klipper-host-protocol's khp_session (same rx-buffering/dispatch-loop
// shape works for any length-prefixed-CRC-checked-resyncable framing),
// but independently implemented against slp_frame rather than shared
// code -- see slp_transport.h's doc comment for why.
#ifndef SLP_SESSION_H
#define SLP_SESSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "slp_transport.h"
#include "slp_frame.h"

// Must be at least SLP_FRAME_MAX so one maximally-sized frame always
// fits.
#define SLP_SESSION_RXBUF_SIZE 256

struct slp_session {
    struct slp_transport transport;
    struct slp_frame_scanner scanner;
    uint8_t rxbuf[SLP_SESSION_RXBUF_SIZE];
    size_t rxbuf_len;
};

void slp_session_init(struct slp_session *s, struct slp_transport transport);

typedef void (*slp_frame_cb)(void *ctx, uint8_t msg_type
                             , const uint8_t *payload, size_t payload_len);

// Reads whatever the transport has available (waiting up to
// timeout_ms), then extracts and dispatches every complete, correctly-
// versioned frame now sitting in the receive buffer. A frame that fails
// slp_frame_view_init's version check (see slp_frame.h) is dropped, not
// passed to cb -- silently misinterpreting a mismatched-firmware peer's
// fields is exactly what the version check exists to prevent.
//
// Returns the number of frames dispatched (>= 0), or a negative value
// if the transport's read() reported a hard error.
int slp_session_poll(struct slp_session *s, int timeout_ms
                     , slp_frame_cb cb, void *cb_ctx);

// Frames payload (payload_len bytes, <= SLP_FRAME_MAX_PAYLOAD) as
// msg_type and writes it via the transport. Returns false if
// payload_len is out of range or the transport's write() didn't accept
// the whole frame.
bool slp_session_send(struct slp_session *s, uint8_t msg_type
                      , const uint8_t *payload, size_t payload_len);

#endif // slp_session.h
