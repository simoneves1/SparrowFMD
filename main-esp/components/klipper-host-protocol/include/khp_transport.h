// A minimal byte-stream transport interface, so khp_session (the
// message-framing/dispatch loop) doesn't need to know whether it's
// running over a UART, USB CDC-ACM, or -- for testing -- an in-memory
// mock. Implementations: khp_uart_transport.h (real, ESP-IDF UART,
// unverified against hardware -- see its own doc comment) and whatever
// test code constructs directly for unit tests.
#ifndef KHP_TRANSPORT_H
#define KHP_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

struct khp_transport {
    void *ctx;

    // Write len bytes. Returns the number of bytes actually written
    // (may be less than len on a partial write -- caller's
    // responsibility to retry/handle), or a negative value on a
    // hard transport error.
    int (*write)(void *ctx, const uint8_t *data, size_t len);

    // Read up to buf_len bytes, waiting up to timeout_ms for at least
    // one byte to become available. Returns the number of bytes read
    // (0 if the timeout elapsed with nothing available), or a negative
    // value on a hard transport error.
    int (*read)(void *ctx, uint8_t *buf, size_t buf_len, int timeout_ms);
};

#endif // khp_transport.h
