// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "unity.h"
#include "util.h"

void setUp() {}
void tearDown() {}

void test_gettime()
{
    uint64_t start, now;

    start = gettime();
    do {
        now = gettime();
    } while (now == start);

    printf("gettime resolution %" PRIu64 " usec \n", now - start);
}

void bench_gettime()
{
    uint64_t calls = 10000000;
    uint64_t start, elapsed;

    start = gettime();

    for (uint64_t i = 0; i < calls; i++)
        gettime();

    elapsed = gettime() - start;

    printf("%" PRIu64 " calls in %.3f seconds (%.3f nsec per call)\n",
           calls, elapsed * 1e-6, (double)elapsed * 1000 / calls);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_gettime);
    RUN_TEST(bench_gettime);

    return UNITY_END();
}
