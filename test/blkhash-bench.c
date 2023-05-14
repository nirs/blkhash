// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "benchmark.h"
#include "blkhash-config.h"
#include "blkhash.h"
#include "util.h"

struct request {
    unsigned char *data;
    size_t len;
    bool ready;
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

static enum input_type input_type = DATA;
static const char *digest_name = "sha256";
static int timeout_seconds = 1;
static int64_t input_size = 0;
static bool aio;
static int queue_depth = 16;
static int threads = 4;
static int streams = BLKHASH_STREAMS;
static int block_size = 64 * KiB;
static int read_size = 256 * KiB;
static int64_t hole_size = (int64_t)MIN(16 * GiB, SIZE_MAX);

static struct request *requests;
static unsigned current;
static struct blkhash_completion *completions;
static struct pollfd poll_fds[1];
static int64_t total_size;

static void setup_aio(struct blkhash *h)
{
    int fd = blkhash_aio_completion_fd(h);
    if (fd < 0)
        FAILF("blkhash_aio_completion_fd: %s", strerror(-fd));

    poll_fds[0].fd = fd;
    poll_fds[0].events = POLLIN;

    completions = calloc(queue_depth, sizeof(*completions));
    if (completions == NULL)
        FAILF("calloc: %s", strerror(errno));
}

static void teardown_aio(void)
{
    free(completions);
}

static void setup_requests(void)
{
    long page_size;

    requests = calloc(queue_depth, sizeof(*requests));
    if (requests == NULL)
        FAILF("calloc: %s", strerror(errno));

    /* When using direct I/O, buffers are aligned to page size. */
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
        FAILF("sysconf(_SC_PAGESIZE): %s", strerror(errno));

    for (int i = 0; i < queue_depth; i++) {
        struct request *req = &requests[i];
        int err;

        err = posix_memalign((void **)&req->data, page_size, read_size);
        if (err)
            FAILF("posix_memalign: %s", strerror(err));

        memset(req->data, input_type == DATA ? 0x55 : 0x00, read_size);
        req->ready = true;
    }
}

static void teardown_requests(void)
{
    free(requests);
}

static const char *short_options = ":hi:d:T:s:aq:t:S:b:r:z:";

static struct option long_options[] = {
    {"help",                no_argument,        0,  'h'},
    {"input-type",          required_argument,  0,  'i'},
    {"digest-name",         required_argument,  0,  'd'},
    {"timeout-seconds",     required_argument,  0,  'T'},
    {"input-size",          required_argument,  0,  's'},
    {"aio",                 no_argument,        0,  'a'},
    {"queue-depth",         required_argument,  0,  'q'},
    {"threads",             required_argument,  0,  't'},
    {"streams",             required_argument,  0,  'S'},
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
"                  [-a|--aio] [-q N|--queue-depth N]\n"
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
        case 'a':
            aio = true;
            break;
        case 'q':
            queue_depth = parse_count(optname, optarg);
            break;
        case 't':
            threads = parse_threads(optname, optarg);
            break;
        case 'S':
            streams = parse_threads(optname, optarg);
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

static void read_request(struct request *req, size_t len)
{
    req->len = len;
}

typedef void (*update_fn)(struct blkhash *h, struct request *req);

static void aio_update(struct blkhash *h, struct request *req)
{
    int err = blkhash_aio_update(h, req->data, req->len, req);
    if (err)
        FAILF("blkhash_aio_update: %s", strerror(err));

    req->ready = false;
    total_size += req->len;
}

static void update(struct blkhash *h, struct request *req)
{
    int err = blkhash_update(h, req->data, req->len);
    if (err)
        FAILF("blkhash_update: %s", strerror(err));

    total_size += req->len;
}

static void zero(struct blkhash *h, size_t len)
{
    int err = blkhash_zero(h, len);
    if (err)
        FAILF("blkhash_zero: %s", strerror(err));

    total_size += len;
}

static void complete_aio_updates(struct blkhash *h)
{
    int signaled = 0;
    int count;

    do {
        if (poll(poll_fds, 1, -1) < 0) {
            if (errno != EINTR)
                FAILF("poll: %s", strerror(errno));

            /* EINTR: Benchmark timeout */
            return;
        }

        if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            FAILF("Error on poll fd: %d", poll_fds[0].revents);

        if (poll_fds[0].revents & POLLIN) {
            /* We must read at least 8 bytes from the completion fd. */
            char sink[queue_depth < 8 ? 8 : queue_depth];

            signaled = read(poll_fds[0].fd, &sink, sizeof(sink));
            if (signaled < 0) {
                if (errno != EINTR)
                    FAILF("read: %s", strerror(errno));

                /* EINTR: Benchmark timeout */
                return;
            }
        }
    } while (signaled == 0);

    count = blkhash_aio_completions(h, completions, queue_depth);
    if (count < 0)
        FAILF("blkhash_aio_completions: %s", strerror(-count));

    for (int i = 0; i < count; i++) {
        struct blkhash_completion *cmp = &completions[i];
        struct request *req;

        if (cmp->error)
            FAILF("async update failed: %s", strerror(cmp->error));

        req = cmp->user_data;
        req->ready = true;
    }
}

static void update_size(struct blkhash *h, update_fn fn)
{
    struct request *req = &requests[0];
    int64_t todo = input_size;

    do {
        while (req->ready && todo > read_size) {
            read_request(req, read_size);
            fn(h, req);
            todo -= read_size;
            current = (current + 1) % queue_depth;
            req = &requests[current];
        }
        while (!req->ready)
            complete_aio_updates(h);
    } while (todo > read_size);

    if (todo > 0) {
        read_request(req, todo);
        fn(h, req);
    }
}

static void update_timeout(struct blkhash *h, update_fn fn)
{
    struct request *req = &requests[0];

    do {
        while (req->ready) {
            read_request(req, read_size);
            fn(h, req);
            if (!timer_is_running)
                return;

            current = (current + 1) % queue_depth;
            req = &requests[current];
        }
        if (!req->ready)
            complete_aio_updates(h);
    } while (timer_is_running);
}

static void zero_size(struct blkhash *h)
{
    int64_t todo = input_size;

    while (todo > hole_size) {
        zero(h, hole_size);
        todo -= hole_size;
    }

    if (todo > 0)
        zero(h, todo);
}

static void zero_timeout(struct blkhash *h)
{
    do {
        zero(h, hole_size);
    } while (timer_is_running);
}

int main(int argc, char *argv[])
{
    int64_t start, elapsed;
    struct blkhash *h;
    struct blkhash_opts *opts = NULL;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    char md_hex[BLKHASH_MAX_MD_SIZE * 2 + 1];
    unsigned int len;
    double seconds;
    int err;

    parse_options(argc, argv);
    setup_requests();

    if (input_size == 0)
        start_timer(timeout_seconds);

    start = gettime();

    opts = blkhash_opts_new(digest_name);
    if (opts == NULL)
        FAIL("blkhash_opts_new");

    if (aio) {
        err = blkhash_opts_set_queue_depth(opts, queue_depth);
        if (err)
            FAILF("blkhash_opts_set_queue_depth: %s", strerror(err));
    }

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

    if (aio)
        setup_aio(h);

    if (input_type == HOLE) {
        if (input_size)
            zero_size(h);
        else
            zero_timeout(h);
    } else {
        if (input_size)
            update_size(h, aio ? aio_update : update);
        else
            update_timeout(h, aio ? aio_update : update);
    }

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
    printf("  \"timeout-seconds\": %d,\n", timeout_seconds);
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"aio\": %s,\n", aio ? "true" : "false");
    printf("  \"queue-depth\": %d,\n", queue_depth);
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
    if (aio)
        teardown_aio();
    teardown_requests();
}
