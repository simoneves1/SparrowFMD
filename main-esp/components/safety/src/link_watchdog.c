#include "link_watchdog.h"

void
link_watchdog_init(struct link_watchdog *wd, uint32_t timeout_ms
                   , link_fault_cb on_fault, void *cb_ctx)
{
    wd->timeout_ms = timeout_ms;
    wd->last_feed_ms = 0;
    wd->state = LINK_OK;
    wd->on_fault = on_fault;
    wd->cb_ctx = cb_ctx;
}

void
link_watchdog_feed(struct link_watchdog *wd, uint32_t now_ms)
{
    wd->last_feed_ms = now_ms;
    // Deliberately does not touch wd->state -- a fault stays latched
    // even if a heartbeat comes through afterward. See header comment.
}

enum link_state
link_watchdog_check(struct link_watchdog *wd, uint32_t now_ms)
{
    if (wd->state == LINK_OK) {
        // Unsigned subtraction wraps correctly across a millis()-style
        // counter rollover as long as the real elapsed time is less
        // than UINT32_MAX ms (~49 days) -- true for any timeout value
        // this module would sensibly be configured with.
        uint32_t elapsed = now_ms - wd->last_feed_ms;
        if (elapsed >= wd->timeout_ms) {
            wd->state = LINK_FAULTED;
            if (wd->on_fault)
                wd->on_fault(wd->cb_ctx);
        }
    }
    return wd->state;
}

void
link_watchdog_reset(struct link_watchdog *wd, uint32_t now_ms)
{
    wd->state = LINK_OK;
    wd->last_feed_ms = now_ms;
}
