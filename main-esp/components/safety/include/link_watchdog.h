// Watchdog/heartbeat logic for detecting a stale or dead link to a
// mainboard/toolhead board, per main-esp/src/README.md's "safety"
// module note. This is NOT thermal runaway protection -- that lives on
// the mainboard/toolhead's own firmware (Klipper's MCU code), not here.
// This module's only job is: "have we heard from this link recently
// enough to trust it?" -- and forcing a safe state is the caller's
// response to that answer, not something this module does itself.
//
// One struct link_watchdog monitors one link. Instantiate one per link
// you want watched (e.g. one for the mainboard, one for the toolhead
// board, if main-esp ends up tracking those as separate links).
//
// Deliberately takes the current time as a parameter rather than
// reading a clock itself, so it has no ESP-IDF/FreeRTOS dependency and
// is fully unit-testable -- the caller (which does know how to read a
// real clock, e.g. esp_timer_get_time()) is responsible for that.
//
// A fault LATCHES: once triggered, it stays faulted even if the link
// starts feeding the watchdog again, until the caller explicitly calls
// link_watchdog_reset(). This is deliberate -- a print job shouldn't
// silently resume just because one heartbeat happened to get through
// after a stall on an otherwise flaky connection; recovery should be a
// decision the caller makes (operator acknowledgement, a fresh
// handshake, etc.), not something that happens automatically.
#ifndef LINK_WATCHDOG_H
#define LINK_WATCHDOG_H

#include <stdint.h>

enum link_state {
    LINK_OK,
    LINK_FAULTED,
};

typedef void (*link_fault_cb)(void *ctx);

struct link_watchdog {
    uint32_t timeout_ms;
    uint32_t last_feed_ms;
    enum link_state state;
    link_fault_cb on_fault;
    void *cb_ctx;
};

// on_fault may be NULL if the caller only wants to poll state via
// link_watchdog_check()'s return value rather than get a callback.
void link_watchdog_init(struct link_watchdog *wd, uint32_t timeout_ms
                        , link_fault_cb on_fault, void *cb_ctx);

// Call whenever a message/heartbeat is received from the monitored
// link. Does NOT clear an existing fault -- see the latching note above.
void link_watchdog_feed(struct link_watchdog *wd, uint32_t now_ms);

// Call periodically (e.g. from a timer or main loop) with the current
// time. Transitions LINK_OK -> LINK_FAULTED exactly once, the first
// time now_ms - last_feed_ms exceeds timeout_ms, invoking on_fault() at
// that transition -- not on every subsequent call while still faulted.
// Returns the state after the check.
enum link_state link_watchdog_check(struct link_watchdog *wd, uint32_t now_ms);

// Explicitly clears a fault (if any) and starts a fresh timeout window
// from now_ms. Whether/when this should be called is entirely a
// caller-level policy decision -- this module just provides the primitive.
void link_watchdog_reset(struct link_watchdog *wd, uint32_t now_ms);

#endif // link_watchdog.h
