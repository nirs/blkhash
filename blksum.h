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

#ifndef BLKSUM_H
#define BLKSUM_H

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROG "blksum"

#define DEBUG(fmt, ...)                                             \
    do {                                                            \
        if (debug)                                                  \
            fprintf (stderr, PROG ": " fmt "\n", ## __VA_ARGS__);   \
    } while (0)

#define FAIL(fmt, ...)                                              \
    do {                                                            \
        fprintf (stderr, PROG ": " fmt "\n", ## __VA_ARGS__);       \
        exit(1);                                                    \
    } while (0)

#define FAIL_ERRNO(msg) FAIL("%s: %s", msg, strerror(errno))

extern bool debug;

struct options {
    const char *digest_name;
    size_t read_size;
    size_t block_size;
    size_t segment_size;
    size_t max_workers;
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

struct src *open_file(const char *path);
struct src *open_pipe(int fd);
struct src *open_nbd(const char *uri);
struct src *open_src(const char *filename);

ssize_t src_pread(struct src *s, void *buf, size_t len, int64_t offset);
ssize_t src_read(struct src *s, void *buf, size_t len);
void src_extents(struct src *s, int64_t offset, int64_t length,
                 struct extent **extents, size_t *count);
void src_close(struct src *s);

void simple_checksum(struct src *s, struct options *opt, unsigned char *out);
void parallel_checksum(const char *filename, struct options *opt,
                       unsigned char *out);

#endif /* BLKSUM_H */
