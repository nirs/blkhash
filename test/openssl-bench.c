// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "benchmark.h"
#include "util.h"

static const char *digest_name = "sha256";
static double timeout_seconds = 1.0;
static int64_t input_size = 0;
static int read_size = 1 * MiB;
static unsigned char *buffer;

static const char *short_options = ":hd:T:s:r:";

static struct option long_options[] = {
   {"help",             no_argument,        0,  'h'},
   {"digest-name",      required_argument,  0,  'd'},
   {"timeout-seconds",  required_argument,  0,  'T'},
   {"input-size",       required_argument,  0,  's'},
   {"read-size",        required_argument,  0,  'r'},
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
        "                  [-T N|--timeout-seconds=N]\n"
        "                  [-r N|--read-size N] [-h|--help]\n"
        "\n",
        stderr);

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
        case 'd':
            digest_name = optarg;
            break;
        case 'T':
            timeout_seconds = parse_seconds(optname, optarg);
            break;
        case 's':
            input_size = parse_size(optname, optarg);
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

static int64_t update_by_size(EVP_MD_CTX *ctx)
{
    int64_t todo = input_size;
    int ok;

    while (todo > read_size) {
        ok = EVP_DigestUpdate(ctx, buffer, read_size);
        assert(ok);
        todo -= read_size;
    }

    if (todo > 0) {
        ok = EVP_DigestUpdate(ctx, buffer, todo);
        assert(ok);
    }

    return input_size;
}

static int64_t update_until_timeout(EVP_MD_CTX *ctx)
{
    int64_t done = 0;
    int ok;

    do {
        ok = EVP_DigestUpdate(ctx, buffer, read_size);
        assert(ok);
        done += read_size;
    } while (timer_is_running);

    return done;
}

int main(int argc, char *argv[])
{
    int64_t start, elapsed;
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned char res[EVP_MAX_MD_SIZE];
    char res_hex[EVP_MAX_MD_SIZE * 2 + 1];
    unsigned int len;
    int64_t total_size;
    double seconds;
    int ok;

    parse_options(argc, argv);

    buffer = malloc(read_size);
    assert(buffer);

    memset(buffer, 0x55, read_size);

    if (input_size == 0)
        start_timer(timeout_seconds);

    start = gettime();

    md = lookup_digest(digest_name);
    assert(md);

    ctx = EVP_MD_CTX_new();
    assert(ctx);

    ok = EVP_DigestInit_ex(ctx, md, NULL);
    assert(ok);

    if (input_size)
        total_size = update_by_size(ctx);
    else
        total_size = update_until_timeout(ctx);

    ok = EVP_DigestFinal_ex(ctx, res, &len);
    assert(ok);

    EVP_MD_CTX_free(ctx);

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    format_hex(res, len, res_hex);

    printf("{\n");
    printf("  \"input-type\": \"data\",\n");
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"timeout-seconds\": %.3f,\n", timeout_seconds);
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"threads\": 1,\n");
    printf("  \"total-size\": %" PRIi64 ",\n", total_size);
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 ",\n", (int64_t)(total_size / seconds));
    printf("  \"checksum\": \"%s\"\n", res_hex);
    printf("}\n");

    free(buffer);
}
