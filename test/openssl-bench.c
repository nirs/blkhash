// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <getopt.h>
#include <pthread.h>
#include <string.h>

#include <openssl/evp.h>

#include "benchmark.h"
#include "util.h"
#include "blkhash-config.h"

struct worker {
    unsigned char res[EVP_MAX_MD_SIZE];
    pthread_t thread;
    unsigned char *buffer;
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    int64_t total_size;
    int64_t calls;
    unsigned int len;
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

static const char *digest_name = "sha256";
static int timeout_seconds = 1;
static int64_t input_size = 0;
static int read_size = 256 * KiB;

/* Using multiple threads to test the maxmimum scalability of blkhash. */
static int threads = 1;

static struct worker workers[MAX_THREADS];

static const char *short_options = ":hd:T:s:r:t:";

static struct option long_options[] = {
   {"help",             no_argument,        0,  'h'},
   {"digest-name",      required_argument,  0,  'd'},
   {"timeout-seconds",  required_argument,  0,  'T'},
   {"input-size",       required_argument,  0,  's'},
   {"read-size",        required_argument,  0,  'r'},
   {"threads",          required_argument,  0,  't'},
   {0,              0,                  0,  0},
};

static void usage(int code)
{
    fputs(
        "\n"
        "Benchmark openssl\n"
        "\n"
        "    openssl-bench [-d DIGEST|--digest-name=DIGEST]\n"
        "                  [-T N|--timeout-seconds=N]\n"
        "                  [-s N|--input-size N] [-r N|--read-size N]\n"
        "                  [-t N|--threads N] [-h|--help]\n"
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
        case 't':
            threads = parse_threads(optname, optarg);
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

static void update_by_size(struct worker *w)
{
    int64_t todo = input_size;

    while (todo > read_size) {
        if (!EVP_DigestUpdate(w->ctx, w->buffer, read_size))
            FAIL("EVP_DigestUpdate");
        todo -= read_size;
        w->total_size += read_size;
        w->calls++;
    }

    if (todo > 0) {
        if (!EVP_DigestUpdate(w->ctx, w->buffer, todo))
            FAIL("EVP_DigestUpdate");
        w->total_size += read_size;
        w->calls++;
    }
}

static void update_until_timeout(struct worker *w)
{
    do {
        if (!EVP_DigestUpdate(w->ctx, w->buffer, read_size))
            FAIL("EVP_DigestUpdate");
        w->total_size += read_size;
        w->calls++;
    } while (timer_is_running);
}

static void *worker_thread(void *arg)
{
    struct worker *w = arg;

    w->buffer = malloc(read_size);
    if (w->buffer == NULL)
        FAIL("malloc");

    memset(w->buffer, 0x55, read_size);

    w->md = create_digest(digest_name);
    if (w->md == NULL)
        FAIL("create_digest");

    w->ctx = EVP_MD_CTX_new();
    if (w->ctx == NULL)
        FAIL("EVP_MD_CTX_new");

    if (!EVP_DigestInit_ex(w->ctx, w->md, NULL))
        FAIL("EVP_DigestInit_ex");

    if (input_size)
        update_by_size(w);
    else
        update_until_timeout(w);

    if (!EVP_DigestFinal_ex(w->ctx, w->res, &w->len))
        FAIL("EVP_DigestFinal_ex");

    EVP_MD_CTX_free(w->ctx);
    free_digest(w->md);
    free(w->buffer);

    return NULL;
}

int main(int argc, char *argv[])
{
    char res_hex[EVP_MAX_MD_SIZE * 2 + 1];
    int64_t start, elapsed;
    int64_t total_size = 0;
    int64_t calls = 0;
    double seconds;
    int err;

    parse_options(argc, argv);

    if (input_size == 0)
        start_timer(timeout_seconds);

    start = gettime();

    for (int i = 0; i < threads; i++) {
        struct worker *w = &workers[i];
        err = pthread_create(&w->thread, NULL, worker_thread, w);
        if (err)
            FAILF("pthread_create: %s", strerror(err));
    }

    for (int i = 0; i < threads; i++) {
        struct worker *w = &workers[i];
        err = pthread_join(w->thread, NULL);
        if (err)
            FAILF("pthread_join: %s", strerror(err));

        total_size += w->total_size;
        calls += w->calls;
    }

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    format_hex(workers[0].res, workers[0].len, res_hex);

    printf("{\n");
    printf("  \"input-type\": \"data\",\n");
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"timeout-seconds\": %d,\n", timeout_seconds);
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"threads\": %d,\n", threads);
    printf("  \"total-size\": %" PRIi64 ",\n", total_size);
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 ",\n", (int64_t)(total_size / seconds));
    printf("  \"kiops\": %.3f,\n", calls / seconds / 1000);
    printf("  \"gips\": %.3f,\n", total_size / seconds / GiB);
    printf("  \"checksum\": \"%s\"\n", res_hex);
    printf("}\n");
}
