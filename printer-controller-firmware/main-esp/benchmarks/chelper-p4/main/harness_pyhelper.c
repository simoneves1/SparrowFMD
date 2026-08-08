// Portable stand-ins for klippy/chelper/pyhelper.c's Python-glue symbols,
// backed by ESP-IDF APIs instead of sys/prctl.h + CPython's logging hook.
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "pyhelper.h"

static const char *TAG = "chelper";

double
get_monotonic(void)
{
    return (double)esp_timer_get_time() * 1e-6;
}

struct timespec
fill_time(double time)
{
    time_t t = (time_t)time;
    struct timespec ts = { t, (long)((time - (double)t) * 1000000000.) };
    return ts;
}

void
set_python_logging_callback(void (*func)(const char *))
{
    (void)func;
}

void
errorf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ESP_LOGE(TAG, "%s", buf);
}

void
report_errno(char *where, int rc)
{
    ESP_LOGE(TAG, "error in %s: %d", where, rc);
}

char *
dump_string(char *outbuf, int outbuf_size, char *inbuf, int inbuf_size)
{
    int n = inbuf_size < outbuf_size - 1 ? inbuf_size : outbuf_size - 1;
    memcpy(outbuf, inbuf, n);
    outbuf[n] = '\0';
    return outbuf;
}

int
set_thread_name(char name[16])
{
    (void)name;
    return 0;
}
