// Portable stand-ins for klippy/chelper/pyhelper.c's Python-glue symbols.
// The vendored chelper sources call these; the real implementations use
// sys/prctl.h and CPython's logging hook, neither of which apply here.
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "pyhelper.h"

double
get_monotonic(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
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
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
report_errno(char *where, int rc)
{
    fprintf(stderr, "error in %s: %d\n", where, rc);
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
