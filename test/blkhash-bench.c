// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blkhash.h"
#include "util.h"

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
static int64_t input_size = 2 * GiB;
static const char *digest_name = "sha256";
static int block_size = 64 * KiB;
static int read_size = 1 * MiB;
static int64_t hole_size = (int64_t)MIN(16 * GiB, SIZE_MAX);
static int threads = 4;

static const char *short_options = ":hi:s:d:b:r:z:t:";

static struct option long_options[] = {
   {"help",         no_argument,        0,  'h'},
   {"input-type",   required_argument,  0,  'i'},
   {"input-size",   required_argument,  0,  's'},
   {"digest-name",  required_argument,  0,  'd'},
   {"block-size",   required_argument,  0,  'b'},
   {"read-size",    required_argument,  0,  'r'},
   {"hole-size",    required_argument,  0,  'z'},
   {"threads",      required_argument,  0,  't'},
   {0,              0,                  0,  0},
};

static void usage(int code)
{
    fputs(
        "\n"
        "Benchmark blkhash\n"
        "\n"
        "    blkhash-bench [-i TYPE|--input-type TYPE] [-s N|--input-size N]\n"
        "                  [-d DIGEST|--digest-name=DIGEST] [-b N|--block-size N]\n"
        "                  [-r N|--read-size N] [-z N|--hole-size N]\n"
        "                  [-t N|--threads N] [-h|--help]\n"
        "\n"
        "input types:\n"
        "    data: non-zero data\n"
        "    zero: all zeros data\n"
        "    hole: unallocated area\n"
        "\n",
        stderr);

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

    fprintf(stderr, "Invalid value for option %s: '%s'\n", name, arg);
    exit(EXIT_FAILURE);
}

static int64_t parse_size(const char *name, const char *arg)
{
    int64_t value;

    value = parse_humansize(arg);
    if (value < 1 || value == -EINVAL) {
        fprintf(stderr, "Invalid value for option %s: '%s'\n", name, arg);
        exit(EXIT_FAILURE);
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
        case 's':
            input_size = parse_size(optname, optarg);
            break;
        case 'd':
            digest_name = optarg;
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
        case 't':
            threads = parse_size(optname, optarg);
            break;
        case ':':
            fprintf(stderr, "Option %s requires an argument", optname);
            exit(EXIT_FAILURE);
            break;
        case '?':
        default:
            fprintf(stderr, "Invalid option: %s", optname);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    unsigned char *buffer = NULL;
    size_t chunk_size;
    uint64_t todo;
    int64_t start, elapsed;
    struct blkhash *h;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    char md_hex[BLKHASH_MAX_MD_SIZE * 2 + 1];
    unsigned int len;
    double seconds;
    int64_t throughput;
    int err = 0;

    parse_options(argc, argv);

    if (input_type != HOLE) {
        buffer = malloc(read_size);
        assert(buffer);
        memset(buffer, input_type == DATA ? 0x55 : 0x00, read_size);
    }

    todo = input_size;
    chunk_size = MIN(input_size,
                     input_type == HOLE ? hole_size : (int64_t)read_size);

    start = gettime();

    h = blkhash_new(digest_name, block_size, threads);
    assert(h);

    while (todo >= chunk_size) {
        if (input_type == HOLE)
            err = blkhash_zero(h, chunk_size);
        else
            err = blkhash_update(h, buffer, chunk_size);
        assert(err == 0);
        todo -= chunk_size;
    }

    if (todo > 0) {
        if (input_type == HOLE)
            err = blkhash_zero(h, todo);
        else
            err = blkhash_update(h, buffer, todo);
        assert(err == 0);
    }

    err = blkhash_final(h, md, &len);
    assert(err == 0);

    blkhash_free(h);

    elapsed = gettime() - start;

    free(buffer);

    seconds = elapsed / 1e6;
    throughput = input_size / seconds;

    format_hex(md, len, md_hex);

    printf("{\n");
    printf("  \"input-type\": \"%s\",\n", type_name(input_type));
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"block-size\": %d,\n", block_size);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"hole-size\": %" PRIi64 ",\n", hole_size);
    printf("  \"threads\": %d,\n", threads);
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 ",\n", throughput);
    printf("  \"checksum\": \"%s\"\n", md_hex);
    printf("}\n");
}
