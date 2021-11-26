/*
 * blkhash - block based hash
 * Copyright Nir Soffer <nirsof@gmail.com>.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blksum.h"

struct file_src {
    struct src src;
    int fd;
};

static ssize_t file_ops_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    struct file_src *fs = (struct file_src *)s;
    size_t pos = 0;

    if (offset + len > s->size)
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

static void file_ops_close(struct src *s)
{
    struct file_src *fs = (struct file_src *)s;

    DEBUG("Closing FILE %s", fs->src.uri);

    close(fs->fd);
    free((char *)fs->src.uri);
    free(fs);
}

static struct src_ops file_ops = {
    .pread = file_ops_pread,
    .close = file_ops_close,
};

struct src *open_file(const char *path)
{
    int fd;
    struct file_src *fs;
    const char *uri;
    off_t size;

    uri = strdup(path);
    if (uri == NULL)
        FAIL_ERRNO("strdup");

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
    fs->src.uri = uri;
    fs->src.format = "raw";
    fs->src.size = size;
    fs->fd = fd;

    return &fs->src;
}
