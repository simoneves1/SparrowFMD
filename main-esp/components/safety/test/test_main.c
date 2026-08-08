// Host-buildable unit tests for the safety module's link_watchdog.
// Build with any C compiler, no ESP-IDF required:
//   gcc -Wall -I../include -o watchdog_test test_main.c ../src/link_watchdog.c
#include <stdio.h>
#include "link_watchdog.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

static void
test_starts_ok(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    CHECK("watchdog starts in LINK_OK", link_watchdog_check(&wd, 0) == LINK_OK);
}

static void
test_stays_ok_before_timeout(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    link_watchdog_feed(&wd, 500);
    CHECK("watchdog stays OK just before the timeout elapses"
         , link_watchdog_check(&wd, 500 + 999) == LINK_OK);
}

static void
test_faults_exactly_at_timeout(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    link_watchdog_feed(&wd, 500);
    CHECK("watchdog faults at exactly the timeout boundary (>=, not >)"
         , link_watchdog_check(&wd, 500 + 1000) == LINK_FAULTED);
}

static void
test_regular_feeds_keep_it_ok_indefinitely(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    enum link_state st = LINK_OK;
    for (uint32_t t = 0; t < 10000; t += 200) {
        link_watchdog_feed(&wd, t);
        st = link_watchdog_check(&wd, t + 100);
    }
    CHECK("regular feeds well under the timeout never fault"
         , st == LINK_OK);
}

struct fault_counter {
    int count;
};

static void
count_fault(void *ctx)
{
    struct fault_counter *c = ctx;
    c->count++;
}

static void
test_fault_callback_fires_exactly_once(void)
{
    struct fault_counter counter = {0};
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, count_fault, &counter);
    link_watchdog_feed(&wd, 0);

    link_watchdog_check(&wd, 500);   // before timeout -- no callback yet
    link_watchdog_check(&wd, 1000);  // crosses the threshold -- one callback
    link_watchdog_check(&wd, 2000);  // still faulted -- no additional callback
    link_watchdog_check(&wd, 3000);  // still faulted -- no additional callback

    CHECK("fault callback fires exactly once across repeated checks"
         , counter.count == 1);
}

static void
test_null_callback_does_not_crash(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 100, NULL, NULL);
    link_watchdog_feed(&wd, 0);
    CHECK("a NULL on_fault callback is handled safely"
         , link_watchdog_check(&wd, 1000) == LINK_FAULTED);
}

static void
test_feed_after_fault_does_not_clear_it(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    link_watchdog_feed(&wd, 0);
    CHECK("watchdog faults after the timeout"
         , link_watchdog_check(&wd, 1000) == LINK_FAULTED);

    // A heartbeat comes through again after the fault -- should NOT
    // silently clear it (see the header comment on latching).
    link_watchdog_feed(&wd, 1001);
    CHECK("feeding a faulted watchdog does not clear the fault"
         , link_watchdog_check(&wd, 1002) == LINK_FAULTED);
}

static void
test_explicit_reset_clears_fault(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    link_watchdog_feed(&wd, 0);
    link_watchdog_check(&wd, 1000);
    CHECK("watchdog is faulted before reset"
         , link_watchdog_check(&wd, 1500) == LINK_FAULTED);

    link_watchdog_reset(&wd, 2000);
    CHECK("explicit reset clears the fault", link_watchdog_check(&wd, 2000) == LINK_OK);
    CHECK("reset starts a fresh timeout window (not immediately faulted)"
         , link_watchdog_check(&wd, 2000 + 999) == LINK_OK);
    CHECK("the fresh window still faults on its own timeout"
         , link_watchdog_check(&wd, 2000 + 1000) == LINK_FAULTED);
}

static void
test_time_wraparound(void)
{
    // last_feed_ms very close to UINT32_MAX, now_ms wrapped around to a
    // small value -- unsigned subtraction should still compute the
    // correct (small) elapsed time.
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    link_watchdog_feed(&wd, 0xFFFFFFFFu - 100); // 100ms before UINT32_MAX
    uint32_t now = 50; // 151ms after last_feed_ms, post-wraparound

    CHECK("time arithmetic across a millis() wraparound stays correct"
         , link_watchdog_check(&wd, now) == LINK_OK);

    uint32_t later = 900; // 1001ms after last_feed_ms
    CHECK("...and still correctly detects a timeout after wraparound"
         , link_watchdog_check(&wd, later) == LINK_FAULTED);
}

int
main(void)
{
    test_starts_ok();
    test_stays_ok_before_timeout();
    test_faults_exactly_at_timeout();
    test_regular_feeds_keep_it_ok_indefinitely();
    test_fault_callback_fires_exactly_once();
    test_null_callback_does_not_crash();
    test_feed_after_fault_does_not_clear_it();
    test_explicit_reset_clears_fault();
    test_time_wraparound();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
