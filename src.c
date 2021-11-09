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

#include <stdlib.h>

#include "blksum.h"

static bool is_nbd_uri(const char *s)
{
    /*
     * libbnd supports more options like TLS and vsock, but I'm not sure
     * these are relevant to this tool.
     */
    return strncmp(s, "nbd://", 6) == 0 ||
           strncmp(s, "nbd+unix:///", 12) == 0;
}

struct src *open_src(const char *filename, bool nbd_server)
{
    /* If the user run the nbd server use it. */
    if (is_nbd_uri(filename)) {
#ifdef HAVE_NBD
        return open_nbd(filename);
#else
        FAIL("NBD is not supported");
#endif
    }

#ifdef HAVE_NBD
    /*
     * If we can run nbd server, start a server and return nbd source
     * connected to the server.
     */
    if (nbd_server) {
        const char *format = probe_format(filename);
        return open_nbd_server(filename, format);
    }
#endif

    /* Othewise open as a raw image. */
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

void src_close(struct src *s)
{
    s->ops->close(s);
}
