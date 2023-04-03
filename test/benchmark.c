// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "benchmark.h"
#include "util.h"

volatile sig_atomic_t timer_is_running;

const char *type_name(enum input_type type)
{
    switch (type) {
        case DATA: return "data";
        case ZERO: return "zero";
        case HOLE: return "hole";
    default:
        return "unknown";
    }
}

int parse_type(const char *name, const char *arg)
{
    if (strcmp(arg, "data") == 0)
        return DATA;

    if (strcmp(arg, "zero") == 0)
        return ZERO;

    if (strcmp(arg, "hole") == 0)
        return HOLE;

    FAILF("Invalid value for option %s: '%s'", name, arg);
}

double parse_seconds(const char *name, const char *arg)
{
    char *end;
    double value;

    value = strtod(arg, &end);
    if (*end != '\0' || value < 0.0) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}

int parse_count(const char *name, const char *arg)
{
    char *end;
    long value;

    value = strtol(arg, &end, 10);
    if (*end != '\0' || value < 1) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}

int64_t parse_size(const char *name, const char *arg)
{
    int64_t value;

    value = parse_humansize(arg);
    if (value < 1 || value == -EINVAL) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}

static void handle_timeout()
{
    timer_is_running = 0;
}

void start_timer(double seconds)
{
    sigset_t all;
    struct sigaction act = {0};
    struct itimerspec it = {0};
    timer_t timer;

    assert(timer_is_running == 0);

    sigfillset(&all);

    act.sa_handler = handle_timeout;
    act.sa_mask = all;

    if (sigaction(SIGALRM, &act, NULL) != 0)
        FAIL("sigaction");

    it.it_value.tv_sec = (int)seconds;
    it.it_value.tv_nsec = (seconds - (int)seconds) * 1000000000;

    /* Zero timeval disarms the timer - use 1 nanosecond timeout. */
    if (it.it_value.tv_sec == 0 && it.it_value.tv_nsec == 0)
        it.it_value.tv_nsec = 1;

    if (timer_create(CLOCK_MONOTONIC, NULL, &timer))
        FAIL("timer_create");

    timer_is_running = 1;

    if (timer_settime(timer, 0, &it, NULL))
        FAIL("setitimer");
}
