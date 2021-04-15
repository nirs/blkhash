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

/* Size of image segment. */
size_t segment_size = 128 * 1024 * 1024;

bool debug = false;
const char *digest_name;
const char *filename;

static void format_hex(unsigned char *md, unsigned int len, char *s)
{
    for (int i = 0; i < len; i++) {
        snprintf(&s[i * 2], 3, "%02x", md[i]);
    }
    s[len * 2] = 0;
}

static inline ssize_t src_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    return s->ops->pread(s, buf, len, offset);
}

static inline ssize_t src_read(struct src *s, void *buf, size_t len)
{
    return s->ops->read(s, buf, len);
}

static inline void src_extents(struct src *s, int64_t offset, int64_t length,
                              struct extent **extents, size_t *count)
{
    if (s->can_extents) {
        DEBUG("get extents offset=%ld length=%ld", offset, length);
        if (s->ops->extents(s, offset, length, extents, count) == 0) {
            DEBUG("got %lu extents", *count);
            return;
        }
    }

    /*
     * If getting extents failed or source does not support extents,
     * fallback to single data extent.
     */

    struct extent *fallback = malloc(sizeof(*fallback));
    if (fallback == NULL) {
        perror("malloc");
        exit(1);
    }

    fallback->length = length;
    fallback->zero = false;

    *extents = fallback;
    *count = 1;
}

static inline void src_close(struct src *s)
{
    s->ops->close(s);
}

static void process_extent(struct src *s, struct blkhash *h, void *buf,
                           int64_t offset, struct extent *extent)
{
    DEBUG("process extent offset=%ld length=%u zero=%d",
          offset, extent->length, extent->zero);

    if (extent->zero) {
        blkhash_zero(h, extent->length);
    } else {
        uint32_t todo = extent->length;
        while (todo) {
            size_t n = (todo > read_size) ? read_size : todo;
            src_pread(s, buf, n, offset);
            blkhash_update(h, buf, n);
            offset += n;
            todo -= n;
        }
    }
}

static void process_segment(struct src *s, struct blkhash *h, void *buf,
                            int64_t offset)
{
    struct extent *extents = NULL;
    size_t count = 0;
    size_t length = (offset + segment_size <= s->size)
        ? segment_size : s->size - offset;
    struct extent *last;
    struct extent *cur;

    src_extents(s, offset, length, &extents, &count);

    last = &extents[0];
    DEBUG("extent 0 length=%u zero=%d", last->length, last->zero);

    for (int i = 1; i < count; i++) {
        cur = &extents[i];
        DEBUG("extent %d length=%u zero=%d", i, cur->length, cur->zero);

        if (cur->zero == last->zero) {
            DEBUG("merge length=%u zero=%d", cur->length, cur->zero);
            last->length += cur->length;
            continue;
        }

        process_extent(s, h, buf, offset, last);
        offset += last->length;
        last = cur;
    }

    if (last->length > 0) {
        process_extent(s, h, buf, offset, last);
        offset += last->length;
    }

    free(extents);
}

static void process_full(struct src *s, struct blkhash *h, void *buf,
                         EVP_MD_CTX *root_ctx)
{
    unsigned char seg_md[EVP_MAX_MD_SIZE];
    unsigned int seg_len;

    size_t segment_count = (s->size + segment_size - 1) / segment_size;

    for (size_t segment = 0; segment < segment_count; segment++) {
        int64_t offset = segment * segment_size;

        process_segment(s, h, buf, offset);
        blkhash_final(h, seg_md, &seg_len);
        blkhash_reset(h);

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, seg_len, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  segment, offset, hex);
        }

        EVP_DigestUpdate(root_ctx, seg_md, seg_len);
    }
}

static void process_eof(struct src *s, struct blkhash *h, void *buf,
                        EVP_MD_CTX *root_ctx)
{
    unsigned char seg_md[EVP_MAX_MD_SIZE];
    unsigned int seg_len;

    for (size_t segment = 0;; segment++) {
        size_t pos = 0;

        while (pos < segment_size) {
            size_t count = src_read(s, buf, read_size);
            if (count == 0)
                break;

            blkhash_update(h, buf, count);
            pos += count;
        }

        if (pos == 0)
            break;

        blkhash_final(h, seg_md, &seg_len);
        blkhash_reset(h);

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, seg_len, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  segment, segment * segment_size, hex);
        }

        EVP_DigestUpdate(root_ctx, seg_md, seg_len);
    }
}

static void checksum(struct src *s, unsigned char *md, unsigned int *len)
{
    struct blkhash *h;
    void *buf;
    const EVP_MD *root_md;
    EVP_MD_CTX *root_ctx;

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

    root_md = EVP_get_digestbyname(digest_name);
    if (root_md == NULL) {
        perror("EVP_get_digestbyname");
        exit(1);
    }

    root_ctx = EVP_MD_CTX_new();
    if (root_ctx == NULL) {
        perror("EVP_MD_CTX_new");
        exit(1);
    }

    EVP_DigestInit_ex(root_ctx, root_md, NULL);

    if (s->size != -1)
        process_full(s, h, buf, root_ctx);
    else
        process_eof(s, h, buf, root_ctx);

    EVP_DigestFinal_ex(root_ctx, md, len);

    EVP_MD_CTX_free(root_ctx);
    blkhash_free(h);
    free(buf);
}

static bool is_nbd_uri(const char *s)
{
    /*
     * libbnd supports more options like TLS and vsock, but I'm not sure
     * these are relevant to this tool.
     */
    return strncmp(s, "nbd://", 6) == 0 ||
           strncmp(s, "nbd+unix:///", 12) == 0;
}

int main(int argc, char *argv[])
{
    struct src *s;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    char hex[EVP_MAX_MD_SIZE * 2 + 1];
    unsigned int md_len;

    if (argc < 2) {
        fprintf(stderr, "Usage: blksum digestname [filename]\n");
        exit(2);
    }

    debug = getenv("BLKSUM_DEBUG") != NULL;

    digest_name = argv[1];

    if (argv[2] != NULL) {
        filename = argv[2];

        if (is_nbd_uri(filename)) {
            s = open_nbd(filename);
        } else {
            s = open_file(filename);
        }
    } else {
        filename = "-";
        s = open_pipe(STDIN_FILENO);
    }

    checksum(s, md_value, &md_len);

    format_hex(md_value, md_len, hex);
    printf("%s  %s\n", hex, filename);

    src_close(s);
    exit(0);
}
