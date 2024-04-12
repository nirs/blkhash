// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKSUM_H
#define BLKSUM_H

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#define PROG "blksum"

#define DEBUG(fmt, ...)                                               \
    do {                                                              \
        if (debug)                                                    \
            fprintf(stderr, "[%10.6f] " fmt "\n",                     \
                    (gettime() - started) * 1e-6, ## __VA_ARGS__);    \
    } while (0)

#define ERROR(fmt, ...) fprintf(stderr, PROG ": " fmt "\n", ## __VA_ARGS__)

void fail(const char *fmt, ...);
bool running(void);

#define FAIL(fmt, ...) fail(PROG ": " fmt "\n", ## __VA_ARGS__)
#define FAIL_ERRNO(msg) FAIL("%s: %s", msg, strerror(errno))

extern bool debug;
extern uint64_t started;

/* Options flags. */
#define USER_QUEUE_DEPTH (1 << 0)
#define USER_READ_SIZE   (1 << 1)
#define USER_CACHE       (1 << 2)

struct src;

struct options {
    const char *digest_name;
    size_t read_size;
    size_t queue_depth;
    size_t block_size;
    unsigned threads;
    int64_t extents_size;
    const char *aio;
    bool cache;
    const char *filename;
    bool progress;
    uint32_t flags;
};

struct server_options {
    const char *filename;
    const char *format;
    const char *aio;
    bool cache;
};

struct nbd_server {
    char *tmpdir;
    char *sock;
    pid_t pid;
};

struct file_info {
    const char *format;
    const char *fs_name;
};

struct extent {
    uint32_t length;
    bool zero;
};

void list_digests(void);

int probe_file(const char *path, struct file_info *fi);

struct nbd_server *start_nbd_server(struct server_options *opt);
char *nbd_server_uri(struct nbd_server *s);
void stop_nbd_server(struct nbd_server *s);

void checksum(struct src *s, struct options *opt, unsigned char *out,
              unsigned int *len);
void aio_checksum(const char *filename, struct options *opt,
                  unsigned char *out, unsigned int *len);

void progress_init(int64_t size);
void progress_update(int64_t len);
void progress_clear();

#endif /* BLKSUM_H */
