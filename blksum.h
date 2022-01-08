// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKSUM_H
#define BLKSUM_H

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define PROG "blksum"

#define DEBUG(fmt, ...)                                             \
    do {                                                            \
        if (debug)                                                  \
            fprintf(stderr, PROG ": " fmt "\n", ## __VA_ARGS__);    \
    } while (0)

#define ERROR(fmt, ...) fprintf(stderr, PROG ": " fmt "\n", ## __VA_ARGS__)

#define FAIL(fmt, ...)                                              \
    do {                                                            \
        fprintf(stderr, PROG ": " fmt "\n", ## __VA_ARGS__);       \
        exit(1);                                                    \
    } while (0)

#define FAIL_ERRNO(msg) FAIL("%s: %s", msg, strerror(errno))

extern bool debug;
extern bool io_only;

struct options {
    const char *digest_name;
    size_t read_size;
    size_t queue_size;
    size_t block_size;
    size_t segment_size;
    size_t workers;
    bool cache;
    const char *filename;
    bool progress;
};

struct server_options {
    const char *filename;
    const char *format;
    bool cache;
    size_t workers;
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

struct progress {
    pthread_mutex_t mutex;
    size_t done;
    size_t count;
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
     * Run source event loop until at least one in-flight command completes.
     * Must be implemented if aio_pread() is available.
     *
     * Return 0 on timeout, 1 if at least one command completed.
     */
    int (*aio_run)(struct src *s, int timeout);

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
int src_aio_run(struct src *s, int timeout);
void src_close(struct src *s);

void simple_checksum(struct src *s, struct options *opt, unsigned char *out);
void parallel_checksum(const char *filename, struct options *opt,
                       unsigned char *out);

struct progress *progress_open(size_t count);
void progress_update(struct progress *p, size_t n);
bool progress_draw(struct progress *p);
void progress_close(struct progress *p);

#endif /* BLKSUM_H */
