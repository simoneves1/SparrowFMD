// A minimal byte-stream transport interface for shared-protocol's
// session layer (slp_session.h), so it doesn't need to know whether
// it's running over a real UART or -- for testing -- an in-memory mock.
// Deliberately independent from klipper-host-protocol's khp_transport
// (same shape, same reasons) rather than a shared abstraction between
// the two -- these are conceptually separate links (main-esp<->
// touch-ui/ams-esp vs. main-esp<->third-party MCU boards), and forcing
// them through one shared transport type would couple two things that
// don't need to be coupled for two call sites. See shared-protocol's
// own README/commit history for the same reasoning applied to its CRC.
#ifndef SLP_TRANSPORT_H
#define SLP_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

struct slp_transport {
    void *ctx;

    // Write len bytes. Returns bytes actually written (may be less than
    // len on a partial write), or negative on a hard transport error.
    int (*write)(void *ctx, const uint8_t *data, size_t len);

    // Read up to buf_len bytes, waiting up to timeout_ms for at least
    // one byte. Returns bytes read (0 if the timeout elapsed with
    // nothing available), or negative on a hard transport error.
    int (*read)(void *ctx, uint8_t *buf, size_t buf_len, int timeout_ms);
};

#endif // slp_transport.h
