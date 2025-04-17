// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef SRC_H
#define SRC_H

#include <stdlib.h>

#include "blksum.h"

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
     * May read less then len bytes. Empty read signals end of file. Returns
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
     * Get image extents for a byte range, up to count extents.
     *
     * Up to count extents will be copied into the extents array. count
     * must not be 0.
     *
     * The number and range of returned extents are:
     * - At least one extent is returned.
     * - The first extent starts at specified offset.
     * - The last extent may end before offset + length.
     */
    int (*extents)(struct src *s, int64_t offset, int64_t length,
                   struct extent *extents, size_t *count);

    /*
     * Close the source.
     */
    void (*close)(struct src *s);
};

struct src *open_file(const char *path);
struct src *open_pipe(int fd);
struct src *open_nbd(const char *uri);
bool is_nbd_uri(const char *s);
struct src *open_src(const char *filename);

static inline ssize_t src_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    return s->ops->pread(s, buf, len, offset);
}

static inline ssize_t src_read(struct src *s, void *buf, size_t len)
{
    return s->ops->read(s, buf, len);
}

static inline void src_extents(struct src *s, int64_t offset, int64_t length,
                               struct extent *extents, size_t *count)
{
    if (s->can_extents) {
        if (s->ops->extents(s, offset, length, extents, count) == 0) {
            return;
        }
    }

    /* Safe fallback: single data extent. */
    extents[0].length = length;
    extents[0].zero = false;
    *count = 1;
}

static inline int src_aio_pread(struct src *s, void *buf, size_t len, int64_t offset,
                                completion_callback cb, void* user_data)
{
    return s->ops->aio_pread(s, buf, len, offset, cb, user_data);
}

static inline int src_aio_prepare(struct src *s, struct pollfd *pfd)
{
    if (s->ops->aio_prepare)
        return s->ops->aio_prepare(s, pfd);

    pfd->fd = -1; /* Don't poll in this iteration. */
    return 0;
}

static inline int src_aio_notify(struct src *s, struct pollfd *pfd)
{
    return s->ops->aio_notify(s, pfd);
}

static inline void src_close(struct src *s)
{
    s->ops->close(s);
}

#endif /* SRC_H */
