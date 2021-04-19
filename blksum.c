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
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/evp.h>
#include "blkhash.h"
#include "blksum.h"
#include "util.h"

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
size_t max_workers = 4;

/* Message digest used to compute the hash. */
const EVP_MD *digest;

/* Size of segment digest. */
int digest_size;

/* Size of source image. */
int64_t src_size;

/* Number of segments in image when computing parallel checksum. */
size_t segment_count;

/* Mutex protecting segment. */
pthread_mutex_t segment_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Next segment to process. */
size_t segment = 0;

/*
 * Buffer containing message digest for every image segment when
 * computing parallel checksum. Workers copy digests into this buffer
 * using the current segment index.
 */
unsigned char *digests_buffer;

static bool is_nbd_uri(const char *s)
{
    /*
     * libbnd supports more options like TLS and vsock, but I'm not sure
     * these are relevant to this tool.
     */
    return strncmp(s, "nbd://", 6) == 0 ||
           strncmp(s, "nbd+unix:///", 12) == 0;
}

static struct src *open_src(const char *filename)
{
    if (is_nbd_uri(filename)) {
        return open_nbd(filename);
    } else {
        return open_file(filename);
    }
}

static ssize_t next_segment()
{
    int err;
    ssize_t index = -1;

    err = pthread_mutex_lock(&segment_mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    if (segment < segment_count) {
        index = segment++;
    }

    err = pthread_mutex_unlock(&segment_mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));

    return index;
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
    if (fallback == NULL)
        FAIL_ERRNO("malloc");

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
    size_t length = (offset + segment_size <= src_size)
        ? segment_size : src_size - offset;
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

static void *worker_thread(void *arg)
{
    size_t id = (size_t)arg;
    struct src *s;
    struct blkhash *h;
    void *buf;
    ssize_t i;

    DEBUG("worker %ld started", id);

    s = open_src(filename);

    h = blkhash_new(block_size, digest_name);
    if (h == NULL)
        FAIL_ERRNO("blkhash_new");

    buf = malloc(read_size);
    if (buf == NULL)
        FAIL_ERRNO("malloc");

    while ((i = next_segment()) != -1) {
        int64_t offset = i * segment_size;
        unsigned char *seg_md = &digests_buffer[i * digest_size];

        DEBUG("worker %ld processing segemnt %lu offset %ld",
              id, segment, offset);

        process_segment(s, h, buf, offset);
        blkhash_final(h, seg_md, NULL);
        blkhash_reset(h);
    }

    free(buf);
    blkhash_free(h);
    src_close(s);

    DEBUG("worker %ld finished", id);
    return NULL;
}

static void checksum_parallel(unsigned char *md)
{
    EVP_MD_CTX *root_ctx;
    size_t worker_count;
    pthread_t *workers;
    int err;

    segment_count = (src_size + segment_size - 1) / segment_size;
    worker_count = segment_count < max_workers ? segment_count : max_workers;

    digests_buffer = malloc(segment_count * digest_size);
    if (digests_buffer == NULL)
        FAIL_ERRNO("malloc");

    workers = malloc(worker_count * sizeof(*workers));
    if (workers == NULL)
        FAIL_ERRNO("malloc");

    for (size_t i = 0; i < worker_count; i++) {
        DEBUG("starting worker %ld", i);
        err = pthread_create(&workers[i], NULL, worker_thread, (void *)i);
        if (err)
            FAIL("pthread_create: %s", strerror(err));
    }

    for (int i = 0; i < worker_count; i++) {
        DEBUG("joining worker %d", i);
        err = pthread_join(workers[i], NULL);
        if (err)
            FAIL("pthread_join: %s", strerror(err));
    }

    root_ctx = EVP_MD_CTX_new();
    if (root_ctx == NULL)
        FAIL_ERRNO("EVP_MD_CTX_new");

    EVP_DigestInit_ex(root_ctx, digest, NULL);

    for (size_t i = 0; i < segment_count; i++) {
        unsigned char *seg_md = &digests_buffer[i * digest_size];

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, digest_size, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  i, i * segment_size, hex);
        }

        EVP_DigestUpdate(root_ctx, seg_md, digest_size);
    }

    EVP_DigestFinal_ex(root_ctx, md, NULL);

    EVP_MD_CTX_free(root_ctx);
    free(digests_buffer);
    free(workers);
}

static void checksum_pipe(unsigned char *md)
{
    struct src *s;
    struct blkhash *h;
    void *buf;
    EVP_MD_CTX *root_ctx;
    unsigned char seg_md[EVP_MAX_MD_SIZE];

    s = open_pipe(STDIN_FILENO);

    h = blkhash_new(block_size, digest_name);
    if (h == NULL)
        FAIL_ERRNO("blkhash_new");

    buf = malloc(read_size);
    if (buf == NULL)
        FAIL_ERRNO("malloc");

    root_ctx = EVP_MD_CTX_new();
    if (root_ctx == NULL)
        FAIL_ERRNO("EVP_MD_CTX_new");

    EVP_DigestInit_ex(root_ctx, digest, NULL);

    for (;; segment++) {
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

        blkhash_final(h, seg_md, NULL);
        blkhash_reset(h);

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, digest_size, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  segment, segment * segment_size, hex);
        }

        EVP_DigestUpdate(root_ctx, seg_md, digest_size);
    }

    EVP_DigestFinal_ex(root_ctx, md, NULL);

    EVP_MD_CTX_free(root_ctx);
    free(buf);
    blkhash_free(h);
    src_close(s);
}

int main(int argc, char *argv[])
{
    unsigned char md[EVP_MAX_MD_SIZE];
    char hex[EVP_MAX_MD_SIZE * 2 + 1];

    if (argc < 2) {
        fprintf(stderr, "Usage: blksum digestname [filename]\n");
        exit(2);
    }

    debug = getenv("BLKSUM_DEBUG") != NULL;

    digest_name = argv[1];

    digest = EVP_get_digestbyname(digest_name);
    if (digest == NULL) {
        fprintf(stderr, "Unknown digest: %s\n", digest_name);
        exit(2);
    }

    digest_size = EVP_MD_size(digest);

    if (argv[2] != NULL) {
        struct src *s;

        filename = argv[2];

        s = open_src(filename);
        src_size = s->size;
        src_close(s);

        checksum_parallel(md);
    } else {
        filename = "-";
        checksum_pipe(md);
    }

    format_hex(md, digest_size, hex);
    printf("%s  %s\n", hex, filename);

    return 0;
}
