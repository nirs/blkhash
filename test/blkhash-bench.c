// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "blkhash.h"
#include "util.h"
#include "benchmark.h"

static volatile sig_atomic_t running = 1;

static enum input_type input_type = DATA;
static const char *digest_name = "sha256";
static double timeout_seconds = 1.0;
static int64_t input_size = 0;
static int block_size = 64 * KiB;
static int read_size = 1 * MiB;
static int64_t hole_size = (int64_t)MIN(16 * GiB, SIZE_MAX);
static int threads = 4;
static int streams = BLKHASH_STREAMS;
static unsigned char *buffer;
static int64_t chunk_size;

static const char *short_options = ":hi:d:T:t:S:s:b:r:z:";

static struct option long_options[] = {
    {"help",                no_argument,        0,  'h'},
    {"input-type",          required_argument,  0,  'i'},
    {"digest-name",         required_argument,  0,  'd'},
    {"timeout-seconds",     required_argument,  0,  'T'},
    {"threads",             required_argument,  0,  't'},
    {"streams",             required_argument,  0,  'S'},
    {"input-size",          required_argument,  0,  's'},
    {"block-size",          required_argument,  0,  'b'},
    {"read-size",           required_argument,  0,  'r'},
    {"hole-size",           required_argument,  0,  'z'},
    {0,                     0,                  0,  0},
};

static void usage(int code)
{
    const char *msg =
"\n"
"Benchmark blkhash\n"
"\n"
"    blkhash-bench [-i TYPE|--input-type TYPE]\n"
"                  [-d DIGEST|--digest-name=DIGEST]\n"
"                  [-T N|--timeout-seconds N] [-s N|--input-size N]\n"
"                  [-t N|--threads N] [-S N|--streams N]\n"
"                  [-b N|--block-size N] [-r N|--read-size N]\n"
"                  [-z N|--hole-size N] [-h|--help]\n"
"\n"
"input types:\n"
"    data: non-zero data\n"
"    zero: all zeros data\n"
"    hole: unallocated area\n"
"\n";

    fputs(msg, stderr);
    exit(code);
}

static void parse_options(int argc, char *argv[])
{
    const char *optname;
    int c;

    /* Silence getopt_long error messages. */
    opterr = 0;

    while (1) {
        optname = argv[optind];
        c = getopt_long(argc, argv, short_options, long_options, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 'h':
            usage(0);
            break;
        case 'i':
            input_type = parse_type(optname, optarg);
            break;
        case 'd':
            digest_name = optarg;
            break;
        case 'T':
            timeout_seconds = parse_seconds(optname, optarg);
            break;
        case 's':
            input_size = parse_size(optname, optarg);
            break;
        case 't':
            threads = parse_count(optname, optarg);
            break;
        case 'S':
            streams = parse_count(optname, optarg);
            break;
        case 'b':
            block_size = parse_size(optname, optarg);
            break;
        case 'r':
            read_size = parse_size(optname, optarg);
            break;
        case 'z':
            hole_size = parse_size(optname, optarg);
            break;
        case ':':
            FAILF("Option %s requires an argument", optname);
            break;
        case '?':
        default:
            FAILF("Invalid option: %s", optname);
        }
    }
}

static void handle_timeout()
{
    running = 0;
}

static void start_timeout(void)
{
    sigset_t all;
    struct sigaction act = {0};
    struct itimerspec it = {0};
    timer_t timer;

    sigfillset(&all);

    act.sa_handler = handle_timeout;
    act.sa_mask = all;

    if (sigaction(SIGALRM, &act, NULL) != 0)
        FAIL("sigaction");

    it.it_value.tv_sec = (int)timeout_seconds;
    it.it_value.tv_nsec = (timeout_seconds - (int)timeout_seconds) * 1000000000;

    /* Zero timeval disarms the timer - use 1 nanosecond timeout. */
    if (it.it_value.tv_sec == 0 && it.it_value.tv_nsec == 0)
        it.it_value.tv_nsec = 1;

    if (timer_create(CLOCK_MONOTONIC, NULL, &timer))
        FAIL("timer_create");

    if (timer_settime(timer, 0, &it, NULL))
        FAIL("setitimer");
}

static inline void update_hash(struct blkhash *h, unsigned char *buf, size_t len)
{
    int err;

    if (input_type == HOLE) {
        err = blkhash_zero(h, len);
        if (err)
            FAILF("blkhash_zero: %s", strerror(err));
    } else {
        err = blkhash_update(h, buf, len);
        if (err)
            FAILF("blkhash_update: %s", strerror(err));
    }
}

static int64_t update_by_size(struct blkhash *h)
{
    int64_t todo = input_size;

    while (todo > chunk_size) {
        update_hash(h, buffer, chunk_size);
        todo -= chunk_size;
    }

    if (todo > 0)
        update_hash(h, buffer, todo);

    return input_size;
}

static int64_t update_until_timeout(struct blkhash *h)
{
    int64_t done = 0;

    do {
        update_hash(h, buffer, chunk_size);
        done += chunk_size;
    } while (running);

    return done;
}

int main(int argc, char *argv[])
{
    int64_t start, elapsed;
    struct blkhash *h;
    struct blkhash_opts *opts = NULL;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    char md_hex[BLKHASH_MAX_MD_SIZE * 2 + 1];
    unsigned int len;
    int64_t total_size;
    double seconds;
    int err;

    parse_options(argc, argv);

    if (input_type != HOLE) {
        buffer = malloc(read_size);
        if (buffer == NULL)
            FAIL("malloc");

        memset(buffer, input_type == DATA ? 0x55 : 0x00, read_size);
    }

    chunk_size = input_type == HOLE ? hole_size : read_size;

    if (input_size == 0)
        start_timeout();

    start = gettime();

    opts = blkhash_opts_new(digest_name);
    if (opts == NULL)
        FAIL("blkhash_opts_new");

    err = blkhash_opts_set_block_size(opts, block_size);
    if (err)
        FAILF("blkhash_opts_set_block_size: %s", strerror(err));

    err = blkhash_opts_set_streams(opts, streams);
    if (err)
        FAILF("blkhash_opts_set_streams: %s", strerror(err));

    err = blkhash_opts_set_threads(opts, threads);
    if (err)
        FAILF("blkhash_opts_set_threads: %s", strerror(err));

    h = blkhash_new_opts(opts);
    if (h == NULL)
        FAIL("blkhash_new_opts");

    if (input_size)
        total_size = update_by_size(h);
    else
        total_size = update_until_timeout(h);

    err = blkhash_final(h, md, &len);
    if (err)
        FAILF("blkhash_final: %s", strerror(err));

    blkhash_free(h);

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    format_hex(md, len, md_hex);

    printf("{\n");
    printf("  \"input-type\": \"%s\",\n", type_name(input_type));
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"timeout-seconds\": %.3f,\n", timeout_seconds);
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"block-size\": %d,\n", block_size);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"hole-size\": %" PRIi64 ",\n", hole_size);
    printf("  \"threads\": %d,\n", threads);
    printf("  \"streams\": %d,\n", streams);
    printf("  \"total-size\": %" PRIi64 ",\n", total_size);
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 ",\n", (int64_t)(total_size / seconds));
    printf("  \"checksum\": \"%s\"\n", md_hex);
    printf("}\n");

    blkhash_opts_free(opts);
    free(buffer);
}
