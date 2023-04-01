// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blkhash.h"
#include "util.h"

#define FAILF(fmt, ...) do { \
    fprintf(stderr, fmt "\n", ## __VA_ARGS__); \
    exit(EXIT_FAILURE); \
} while (0)

#define FAIL(msg) do { \
    perror(msg); \
    exit(EXIT_FAILURE); \
} while (0)

static volatile sig_atomic_t running = 1;

enum input_type {DATA, ZERO, HOLE};

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

static enum input_type input_type = DATA;
static const char *digest_name = "sha256";
static int timeout_seconds = 1;
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

static int parse_type(const char *name, const char *arg)
{
    if (strcmp(arg, "data") == 0)
        return DATA;

    if (strcmp(arg, "zero") == 0)
        return ZERO;

    if (strcmp(arg, "hole") == 0)
        return HOLE;

    FAILF("Invalid value for option %s: '%s'", name, arg);
}

static int parse_count(const char *name, const char *arg)
{
    char *end;
    long value;

    value = strtol(arg, &end, 10);
    if (*end != '\0' || value < 1) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}

static int64_t parse_size(const char *name, const char *arg)
{
    int64_t value;

    value = parse_humansize(arg);
    if (value < 1 || value == -EINVAL) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
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
            timeout_seconds = parse_count(optname, optarg);
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
    sigfillset(&all);

    struct sigaction act = { .sa_handler = handle_timeout, .sa_mask = all, };

    if (sigaction(SIGALRM, &act, NULL) != 0)
        FAIL("sigaction");

    alarm(timeout_seconds);
}

static inline void update_hash(struct blkhash *h, unsigned char *buf, size_t len)
{
    int err;

    if (input_type == HOLE)
        err = blkhash_zero(h, len);
    else
        err = blkhash_update(h, buf, len);

    assert(err == 0);
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

    while (running) {
        update_hash(h, buffer, chunk_size);
        done += chunk_size;
    }

    return done;
}

int main(int argc, char *argv[])
{
    int64_t start, elapsed;
    struct blkhash *h;
    struct blkhash_opts *opts;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    char md_hex[BLKHASH_MAX_MD_SIZE * 2 + 1];
    unsigned int len;
    int64_t total_size;
    double seconds;
    int err;

    parse_options(argc, argv);

    if (input_type != HOLE) {
        buffer = malloc(read_size);
        assert(buffer);
        memset(buffer, input_type == DATA ? 0x55 : 0x00, read_size);
    }

    chunk_size = input_type == HOLE ? hole_size : read_size;

    if (timeout_seconds)
        start_timeout();

    start = gettime();

    opts = blkhash_opts_new(digest_name);
    assert(opts);
    err = blkhash_opts_set_block_size(opts, block_size);
    assert(err == 0);
    err = blkhash_opts_set_streams(opts, streams);
    assert(err == 0);
    err = blkhash_opts_set_threads(opts, threads);
    assert(err == 0);

    h = blkhash_new_opts(opts);
    blkhash_opts_free(opts);
    assert(h);

    if (input_size)
        total_size = update_by_size(h);
    else
        total_size = update_until_timeout(h);

    err = blkhash_final(h, md, &len);
    assert(err == 0);

    blkhash_free(h);

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    format_hex(md, len, md_hex);

    printf("{\n");
    printf("  \"input-type\": \"%s\",\n", type_name(input_type));
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"timeout-seconds\": %d,\n", timeout_seconds);
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

    free(buffer);
}
