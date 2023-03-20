// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "util.h"

static int64_t input_size = 512 * MiB;
static const char *digest_name = "sha256";
static int read_size = 1 * MiB;

static const char *short_options = ":hs:d:r:";

static struct option long_options[] = {
   {"help",         no_argument,        0,  'h'},
   {"input-size",   required_argument,  0,  's'},
   {"digest-name",  required_argument,  0,  'd'},
   {"read-size",    required_argument,  0,  'r'},
   {0,              0,                  0,  0},
};

static void usage(int code)
{
    fputs(
        "\n"
        "Benchmark openssl\n"
        "\n"
        "    openssl-bench [-s N|--input-size N]\n"
        "                  [-d DIGEST|--digest-name=DIGEST]\n"
        "                  [-r N|--read-size N] [-h|--help]\n"
        "\n",
        stderr);

    exit(code);
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
        case 's':
            input_size = parse_size(optname, optarg);
            break;
        case 'd':
            digest_name = optarg;
            break;
        case 'r':
            read_size = parse_size(optname, optarg);
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
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned char res[EVP_MAX_MD_SIZE];
    unsigned int len;
    double seconds;
    int64_t throughput;
    int ok;

    parse_options(argc, argv);

    buffer = malloc(read_size);
    assert(buffer);

    memset(buffer, 0x55, read_size);

    todo = input_size;
    chunk_size = MIN(input_size, (int64_t)read_size);

    start = gettime();

    md = lookup_digest(digest_name);
    assert(md);

    ctx = EVP_MD_CTX_new();
    assert(ctx);

    ok = EVP_DigestInit_ex(ctx, md, NULL);
    assert(ok);

    while (todo >= chunk_size) {
        ok = EVP_DigestUpdate(ctx, buffer, chunk_size);
        assert(ok);
        todo -= chunk_size;
    }

    if (todo > 0) {
        ok = EVP_DigestUpdate(ctx, buffer, todo);
        assert(ok);
    }

    ok = EVP_DigestFinal_ex(ctx, res, &len);
    assert(ok);

    EVP_MD_CTX_free(ctx);

    elapsed = gettime() - start;

    free(buffer);

    seconds = elapsed / 1e6;
    throughput = input_size / seconds;

    printf("{\n");
    printf("  \"input-type\": \"data\",\n");
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"threads\": 1,\n");
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 "\n", throughput);
    printf("}\n");
}
