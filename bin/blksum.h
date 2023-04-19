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
extern bool io_only;
extern uint64_t started;

/* Options flags. */
#define USER_QUEUE_DEPTH (1 << 0)
#define USER_READ_SIZE   (1 << 1)
#define USER_CACHE       (1 << 2)

struct options {
    const char *digest_name;
    size_t read_size;
    size_t queue_depth;
    size_t block_size;
    unsigned threads;
    unsigned streams;
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

struct src {
    struct src_ops *ops;

    /*
     * Filename or NBD URI used to open this source. When starting NBD server
     * for local file, this will be the NBD URI, not the filename specified by
     * the user.
     */
    const char *uri;

    int64_t size;
    bool can_extents;
};

struct extent {
    uint32_t length;
    bool zero;
};

/*
 * Callback invoked when an async command completes.
 */
typedef int (*completion_callback)(void *user_data, int *error);

struct src_ops {
    /*
     * Always read exactly len bytes, offset + len must be less than size.
     * Return number of bytes read.  Optional if read() is available.
     */
    ssize_t (*pread)(struct src *s, void *buf, size_t len, int64_t offset);

    /*
     * May read less then len bytes. Empty read singals end of file. Returns
     * the number of bytes read. Optional if pread() is available.
     */
    ssize_t (*read)(struct src *s, void *buf, size_t len);

    /*
     * Always read exactly len bytes, offset + len must be less than
     * size.  When the read completes, cb is invoked with user_data.
     * Optional if read() is available.
     */
    int (*aio_pread)(struct src *s, void *buf, size_t len, int64_t offset,
                     completion_callback cb, void *user_data);

    /*
     * Called before polling for events. The source must set the fd for
     * polling and the wanted events (POLLIN, POLLOUT). The function can
     * set fd to -1 to to disable polling in this iteration.
     *
     * Return 0 on success, -1 on error.
     */
    int (*aio_prepare)(struct src *s, struct pollfd *pfd);

    /*
     * Called after polling when one or more events set in ev_prepare()
     * are available, or when an error was detected (POLLERR, POLLHUP,
     * POLLNVAL).
     *
     * Return 0 on success, -1 on error.
     */
    int (*aio_notify)(struct src *s, struct pollfd *pfd);

    /*
     * Get image extents for a byte range. Caller must free the returned
     * extents list. The number and range of returned extents are:
     * - At least one extent is retruned.
     * - The first extent starts at specified offset.
     * - The last extent may end before offset + length.
     */
    int (*extents)(struct src *s, int64_t offset, int64_t length,
                   struct extent **extents, size_t *count);

    /*
     * Close the source.
     */
    void (*close)(struct src *s);
};

void list_digests(void);

bool is_nbd_uri(const char *s);
int probe_file(const char *path, struct file_info *fi);

struct nbd_server *start_nbd_server(struct server_options *opt);
char *nbd_server_uri(struct nbd_server *s);
void stop_nbd_server(struct nbd_server *s);

struct src *open_file(const char *path);
struct src *open_pipe(int fd);
struct src *open_nbd(const char *uri);
struct src *open_src(const char *filename);

void src_extents(struct src *s, int64_t offset, int64_t length,
                 struct extent **extents, size_t *count);
ssize_t src_read(struct src *s, void *buf, size_t len);
ssize_t src_pread(struct src *s, void *buf, size_t len, int64_t offset);
int src_aio_pread(struct src *s, void *buf, size_t len, int64_t offset,
                  completion_callback cb, void* user_data);
int src_aio_prepare(struct src *s, struct pollfd *pfd);
int src_aio_notify(struct src *s, struct pollfd *pfd);

void src_close(struct src *s);

void checksum(struct src *s, struct options *opt, unsigned char *out);
void aio_checksum(const char *filename, struct options *opt,
                  unsigned char *out);

void progress_init(int64_t size);
void progress_update(int64_t len);
void progress_clear();

#endif /* BLKSUM_H */
