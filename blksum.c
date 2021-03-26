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
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/evp.h>
#include "blkhash.h"
#include "blksum.h"

/*
 * Bigger size is optimal for reading, espcially when reading for remote
 * storage. Should be aligned to block size for best performance.
 * TODO: Requires more testing with different storage.
 */
const size_t read_size = 2 * 1024 * 1024;

/*
 * Smaller size is optimal for hashing and detecting holes. This give best
 * perforamnce when testing on zero image on local nvme drive.
 * TODO: Requires more testing with different storage.
 */
size_t block_size = 64 * 1024;

const char *digest_name;
const char *filename;

static inline ssize_t src_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    return s->ops->pread(s, buf, len, offset);
}

static inline ssize_t src_read(struct src *s, void *buf, size_t len)
{
    return s->ops->read(s, buf, len);
}

static inline void src_close(struct src *s)
{
    s->ops->close(s);
}

static void process_full(struct src *s, struct blkhash *h, void *buf)
{
    int64_t offset = 0;

    while (offset < s->size) {
        size_t n = (offset + read_size <= s->size)
            ? read_size : s->size - offset;

        src_pread(s, buf, n, offset);
        blkhash_update(h, buf, n);

        offset += n;
    }
}

static void process_eof(struct src *s, struct blkhash *h, void *buf)
{
    while (1) {
        ssize_t n = src_read(s, buf, read_size);
        if (n == 0)
            break;

        blkhash_update(h, buf, n);
    }
}

static void blkhash_src(struct src *s, unsigned char *md, unsigned int *len)
{
    struct blkhash *h;
    void *buf;

    h = blkhash_new(block_size, digest_name);
    if (h == NULL) {
        perror("blkhash_new");
        exit(1);
    }

    buf = malloc(read_size);
    if (buf == NULL) {
        perror("malloc");
        exit(1);
    }

    if (s->size != -1)
        process_full(s, h, buf);
    else
        process_eof(s, h, buf);

    blkhash_final(h, md, len);
    blkhash_free(h);
    free(buf);
}

int main(int argc, char *argv[])
{
    struct src *s;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len, i;

    if (argc < 2) {
        fprintf(stderr, "Usage: blksum digestname [filename]\n");
        exit(2);
    }

    digest_name = argv[1];

    if (argv[2] != NULL) {
        struct stat sb;
        filename = argv[2];

        if (stat(filename, &sb) == 0) {
            s = open_file(filename);
        } else {
            /*
             * Not a file, lets try using nbd.
             * TODO: Check that filename is nbd uri.
             */
            s = open_nbd(filename);
        }
    } else {
        filename = "-";
        s = open_pipe(STDIN_FILENO);
    }

    blkhash_src(s, md_value, &md_len);

    for (i = 0; i < md_len; i++)
        printf("%02x", md_value[i]);

    printf("  %s\n", filename);

    src_close(s);
    exit(0);
}
