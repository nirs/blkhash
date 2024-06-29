// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
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

static const char *digest_name = "sha256";
static bool aio;
static int queue_depth = 16;
static int threads = 4;
static int block_size = 64 * KiB;
static int read_size = 256 * KiB;
static const char *filename;

static struct request *requests;
static struct blkhash_completion *completions;
static struct pollfd poll_fds[1];
static int64_t bytes_hashed;
static int64_t calls;

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
    requests = calloc(queue_depth, sizeof(*requests));
    if (requests == NULL)
        FAILF("calloc: %s", strerror(errno));

    for (int i = 0; i < queue_depth; i++) {
        struct request *req = &requests[i];
        req->ready = true;
    }
}

static void teardown_requests(void)
{
    free(requests);
}

static const char *short_options = ":hd:aq:t:b:r:";

static struct option long_options[] = {
    {"help",                no_argument,        0,  'h'},
    {"digest-name",         required_argument,  0,  'd'},
    {"aio",                 no_argument,        0,  'a'},
    {"queue-depth",         required_argument,  0,  'q'},
    {"threads",             required_argument,  0,  't'},
    {"block-size",          required_argument,  0,  'b'},
    {"read-size",           required_argument,  0,  'r'},
    {0,                     0,                  0,  0},
};

static void usage(int code)
{
    const char *msg =
"\n"
"Benchmark blkhash\n"
"\n"
"    mmap-bench [-d DIGEST|--digest-name=DIGEST]\n"
"               [-a|--aio] [-q N|--queue-depth N]\n"
"               [-t N|--threads N] [-b N|--block-size N]\n"
"               [-r N|--read-size N] [-h|--help]\n"
"               filename\n"
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
        case 'd':
            digest_name = optarg;
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
        case 'b':
            block_size = parse_size(optname, optarg);
            break;
        case 'r':
            read_size = parse_size(optname, optarg);
            break;
        case ':':
            FAILF("Option %s requires an argument", optname);
            break;
        case '?':
        default:
            FAILF("Invalid option: %s", optname);
        }
    }

    /* Parse arguments */

    if (optind == argc)
        FAILF("filename required");

    filename = argv[optind++];
}

typedef void (*update_fn)(struct blkhash *h, struct request *req);

static void aio_update(struct blkhash *h, struct request *req)
{
    int err = blkhash_aio_update(h, req->data, req->len, req);
    if (err)
        FAILF("blkhash_aio_update: %s", strerror(err));

    req->ready = false;
}

static void update(struct blkhash *h, struct request *req)
{
    int err = blkhash_update(h, req->data, req->len);
    if (err)
        FAILF("blkhash_update: %s", strerror(err));

    bytes_hashed += req->len;
    calls++;
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
        req->data = 0;
        bytes_hashed += req->len;
        calls++;
    }
}

static void update_hash(struct blkhash *h, unsigned char *start, int64_t size, update_fn fn)
{
    unsigned char *data = start;
    int64_t todo = size;

    while (bytes_hashed < size) {

        /* Hash more data */
        for (int i = 0; i < queue_depth && todo > 0; i++) {
            struct request *req = &requests[i];

            if (req->ready) {
                req->data = data;
                req->len = MIN(todo, read_size);
                data += req->len;
                todo -= req->len;

                fn(h, req);
            }
        }

        /* Wait for complections. */
        if (aio)
            complete_aio_updates(h);
    }
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
    int fd;
    unsigned char *mm;
    struct stat st;
    int err;

    parse_options(argc, argv);

    fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd == -1)
        FAILF("open: %s", strerror(errno));

    err = fstat(fd, &st);
    if (err)
        FAILF("start: %s", strerror(errno));

    mm = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED)
        FAILF("mmap: %s", strerror(errno));

    close(fd);

    setup_requests();

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

    err = blkhash_opts_set_threads(opts, threads);
    if (err)
        FAILF("blkhash_opts_set_threads: %s", strerror(err));

    h = blkhash_new_opts(opts);
    if (h == NULL)
        FAIL("blkhash_new_opts");

    if (aio)
        setup_aio(h);

    update_hash(h, mm, st.st_size, aio ? aio_update : update);

    err = blkhash_final(h, md, &len);
    if (err)
        FAILF("blkhash_final: %s", strerror(err));

    blkhash_free(h);

    err = munmap(mm, st.st_size);
    if (err)
        FAILF("munmap: %s", strerror(errno));

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    format_hex(md, len, md_hex);

    printf("{\n");
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"aio\": %s,\n", aio ? "true" : "false");
    printf("  \"queue-depth\": %d,\n", queue_depth);
    printf("  \"block-size\": %d,\n", block_size);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"threads\": %d,\n", threads);
    printf("  \"total-size\": %" PRIi64 ",\n", bytes_hashed);
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 ",\n", (int64_t)(bytes_hashed / seconds));
    printf("  \"kiops\": %.3f,\n", calls / seconds / 1000);
    printf("  \"gips\": %.3f,\n", bytes_hashed / seconds / GiB);
    printf("  \"checksum\": \"%s\"\n", md_hex);
    printf("}\n");

    blkhash_opts_free(opts);
    if (aio)
        teardown_aio();
    teardown_requests();
}
