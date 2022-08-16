// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "blkhash_internal.h"
#include "util.h"

#define BLOCK_SIZE (64*1024L)

void setUp() {}
void tearDown() {}

static unsigned char *buffer;
static bool quick;

void bench(const char *name, uint64_t size, const void *buf, bool zero)
{
    int64_t start, elapsed;
    double seconds;
    char *hsize, *hrate;
    bool result = !zero;

    if (quick)
        size /= 1024;

    start = gettime();

    for (uint64_t i = 0; i < size / BLOCK_SIZE; i++) {
        result = is_zero(buf, BLOCK_SIZE);
        if (result != zero)
            break;
    }

    elapsed = gettime() - start;

    TEST_ASSERT_EQUAL_INT(zero, result);

    seconds = elapsed / 1e6;
    hsize = humansize(size);
    hrate = humansize(size / seconds);

    printf("%s: %s in %.3f seconds (%s/s)\n", name, hsize, seconds, hrate);

    free(hsize);
    free(hrate);
}

void bench_aligned_data_best()
{
    assert(((uintptr_t)buffer % 8) == 0);
    memset(buffer, 0, BLOCK_SIZE);
    buffer[7] = 0x55;
    bench("aligned data best", 27 * TiB, buffer, false);
}

void bench_aligned_data_worst()
{
    memset(buffer, 0, BLOCK_SIZE);
    buffer[BLOCK_SIZE-1] = 0x55;
    bench("aligned data worst", 60 * GiB, buffer, false);
}

void bench_aligned_zero()
{
    memset(buffer, 0, BLOCK_SIZE);
    bench("aligned zero", 60 * GiB, buffer, true);
}

void bench_unaligned_data_best()
{
    unsigned char *p = buffer + 5;
    assert(((uintptr_t)p % 8) != 0);
    memset(p, 0, BLOCK_SIZE);
    p[7] = 0x55;
    bench("unaligned data best", 27 * TiB, p, false);
}

void bench_unaligned_data_worst()
{
    unsigned char *p = buffer + 5;
    memset(p, 0, BLOCK_SIZE);
    p[BLOCK_SIZE-1] = 0x55;
    bench("unaligned data worst", 60 * GiB, p, false);
}

void bench_unaligned_zero()
{
    unsigned char *p = buffer + 5;
    memset(p, 0, BLOCK_SIZE);
    bench("unaligned zero", 60 * GiB, p, true);
}

int main(int argc, char *argv[])
{
    /* Extract bytes for testing unalinged buffers. */
    buffer = malloc(BLOCK_SIZE + 5);
    assert(buffer);

    /* Minimal test for CI and build machines. */
    quick = (argc > 1 && strcmp(argv[1], "quick") == 0);

    UNITY_BEGIN();

    RUN_TEST(bench_aligned_data_best);
    RUN_TEST(bench_aligned_data_worst);
    RUN_TEST(bench_aligned_zero);

    RUN_TEST(bench_unaligned_data_best);
    RUN_TEST(bench_unaligned_data_worst);
    RUN_TEST(bench_unaligned_zero);

    free(buffer);

    return UNITY_END();
}
