// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "src.h"

struct file_src {
    struct src src;
    int fd;
};

static ssize_t file_ops_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    struct file_src *fs = (struct file_src *)s;
    size_t pos = 0;

    if (offset + (int64_t)len > s->size)
        FAIL("read after end of file offset=%" PRIi64 " len=%zu size=%" PRIi64,
             offset, len, s->size);

    while (pos < len) {
        ssize_t n;

        do {
            n = pread(fs->fd, buf + pos, len - pos, offset + pos);
        } while (n == -1 && errno == EINTR);

        if (n < 0)
            FAIL_ERRNO("pread");

        pos += n;
    }

    return pos;
}

/* Fake async implementation to keep callers happy. */
static int file_ops_aio_pread(struct src *s, void *buf, size_t len,
                              int64_t offset, completion_callback cb,
                              void *user_data)
{
    int error = 0;

    file_ops_pread(s, buf, len, offset);
    cb(user_data, &error);
    return 0;
}

static void file_ops_close(struct src *s)
{
    struct file_src *fs = (struct file_src *)s;

    DEBUG("Closing FILE %s", fs->src.uri);

    close(fs->fd);
    free(fs);
}

static struct src_ops file_ops = {
    .pread = file_ops_pread,
    .aio_pread = file_ops_aio_pread,
    .close = file_ops_close,
};

struct src *open_file(const char *path)
{
    int fd;
    struct file_src *fs;
    off_t size;

    DEBUG("Opening FILE %s", path);

    fd = open(path, O_RDONLY);
    if (fd == -1)
        FAIL_ERRNO("open");

    /* This works with both regular file and block device. */
    size = lseek(fd, 0, SEEK_END);
    if (size == -1)
        FAIL_ERRNO("lseek");

    /* Not required since we use pread() but nicer. */
    if (lseek(fd, 0, SEEK_SET) == -1)
        FAIL_ERRNO("lseek");

    fs = calloc(1, sizeof(*fs));
    if (fs == NULL)
        FAIL_ERRNO("calloc");

#ifdef POSIX_FADV_SEQUENTIAL
    /* Best effort, ignore errors. */
    posix_fadvise(fd, 0, size, POSIX_FADV_SEQUENTIAL);
#endif

    fs->src.ops = &file_ops;
    fs->src.uri = path;
    fs->src.size = size;
    fs->fd = fd;

    return &fs->src;
}
