// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdlib.h>

#include "blksum.h"

bool is_nbd_uri(const char *s)
{
    /*
     * libbnd supports more options like TLS and vsock, but I'm not sure
     * these are relevant to this tool.
     */
    return strncmp(s, "nbd://", 6) == 0 ||
           strncmp(s, "nbd+unix:///", 12) == 0;
}

struct src *open_src(const char *filename)
{
    if (is_nbd_uri(filename)) {
#ifdef HAVE_NBD
        return open_nbd(filename);
#else
        FAIL("NBD is not supported");
#endif
    }

    return open_file(filename);
}

ssize_t src_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    return s->ops->pread(s, buf, len, offset);
}

ssize_t src_read(struct src *s, void *buf, size_t len)
{
    return s->ops->read(s, buf, len);
}

void src_extents(struct src *s, int64_t offset, int64_t length,
                 struct extent **extents, size_t *count)
{
    if (s->can_extents) {
        if (s->ops->extents(s, offset, length, extents, count) == 0) {
            return;
        }
    }

    /*
     * If getting extents failed or source does not support extents,
     * fallback to single data extent.
     */

    struct extent *fallback = malloc(sizeof(*fallback));
    if (fallback == NULL)
        FAIL_ERRNO("malloc");

    fallback->length = length;
    fallback->zero = false;

    *extents = fallback;
    *count = 1;
}

int src_aio_pread(struct src *s, void *buf, size_t len, int64_t offset,
                  completion_callback cb, void* user_data)
{
    return s->ops->aio_pread(s, buf, len, offset, cb, user_data);
}

int src_aio_run(struct src *s, int timeout)
{
    return s->ops->aio_run(s, timeout);
}

void src_close(struct src *s)
{
    s->ops->close(s);
}
