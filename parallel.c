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

#include <pthread.h>
#include <stdlib.h>
#include <openssl/evp.h>

#include "blkhash.h"
#include "blksum.h"
#include "util.h"

struct job {
    /*
     * Source used to get inital image details, and when running NBD server to
     * start and terminate the NBD server.
     */
    struct src *src;

    struct options *opt;
    const EVP_MD *md;
    int md_size;
    size_t segment_count;
    pthread_mutex_t mutex;
    size_t segment;
    unsigned char *digests_buffer;
};

struct worker {
    pthread_t thread;
    int id;
    struct job *job;
    struct src *s;
    struct blkhash *h;
    void *buf;
};

static void init_job(struct job *job, const char *filename,
                     struct options *opt)
{
    int err;

    job->src = open_src(filename);
    job->opt = opt;
    job->md = EVP_get_digestbyname(opt->digest_name);
    if (job->md == NULL)
        FAIL_ERRNO("EVP_get_digestbyname");

    job->md_size = EVP_MD_size(job->md);
    job->segment_count = (job->src->size + opt->segment_size - 1)
        / opt->segment_size;
    err = pthread_mutex_init(&job->mutex, NULL);
    if (err)
        FAIL("pthread_mutex_init: %s", strerror(err));

    job->segment = 0;
    job->digests_buffer = malloc(job->segment_count * job->md_size);
    if (job->digests_buffer == NULL)
        FAIL_ERRNO("malloc");
}

static void destroy_job(struct job *job)
{
    int err;

    src_close(job->src);
    err = pthread_mutex_destroy(&job->mutex);
    if (err)
        FAIL("pthread_mutex_destroy: %s", strerror(err));

    free(job->digests_buffer);
    job->digests_buffer = NULL;
}

static inline unsigned char *segment_md(struct job *job, size_t n)
{
    return &job->digests_buffer[n * job->md_size];
}

static ssize_t next_segment(struct job *job)
{
    int err;
    ssize_t segment = -1;

    err = pthread_mutex_lock(&job->mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    if (job->segment < job->segment_count) {
        segment = job->segment++;
    }

    err = pthread_mutex_unlock(&job->mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));

    return segment;
}

static void compute_root_hash(struct job *job, unsigned char *out)
{
    struct options *opt = job->opt;
    EVP_MD_CTX *md_ctx;

    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL)
        FAIL_ERRNO("EVP_MD_CTX_new");

    EVP_DigestInit_ex(md_ctx, job->md, NULL);

    for (size_t i = 0; i < job->segment_count; i++) {
        unsigned char *seg_md = segment_md(job, i);

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, job->md_size, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  i, i * opt->segment_size, hex);
        }

        EVP_DigestUpdate(md_ctx, seg_md, job->md_size);
    }

    EVP_DigestFinal_ex(md_ctx, out, NULL);

    EVP_MD_CTX_free(md_ctx);
}

static void process_extent(struct worker *w, int64_t offset,
                           struct extent *extent)
{
    struct options *opt = w->job->opt;

    DEBUG("worker %d process extent offset=%" PRIi64" length=%" PRIu32
          " zero=%d",
          w->id, offset, extent->length, extent->zero);

    if (extent->zero) {
        blkhash_zero(w->h, extent->length);
    } else {
        uint32_t todo = extent->length;
        while (todo) {
            size_t n = (todo > opt->read_size) ? opt->read_size : todo;
            src_pread(w->s, w->buf, n, offset);
            blkhash_update(w->h, w->buf, n);
            offset += n;
            todo -= n;
        }
    }
}

static void process_segment(struct worker *w, int64_t offset)
{
    struct job *job = w->job;
    struct options *opt = job->opt;
    struct extent *extents = NULL;
    size_t count = 0;
    size_t length = (offset + opt->segment_size <= job->src->size)
        ? opt->segment_size : job->src->size - offset;
    struct extent *last;
    struct extent *cur;

    DEBUG("worker %d get extents offset=%" PRIi64 " length=%zu",
          w->id, offset, length);

    src_extents(w->s, offset, length, &extents, &count);

    DEBUG("worker %d got %lu extents", w->id, count);

    last = &extents[0];
    DEBUG("worker %d extent 0 length=%u zero=%d",
          w->id, last->length, last->zero);

    for (int i = 1; i < count; i++) {
        cur = &extents[i];
        DEBUG("worker %d extent %d length=%u zero=%d",
              w->id, i, cur->length, cur->zero);

        if (cur->zero == last->zero) {
            DEBUG("worker %d merge length=%u zero=%d",
                  w->id, cur->length, cur->zero);
            last->length += cur->length;
            continue;
        }

        process_extent(w, offset, last);
        offset += last->length;
        last = cur;
    }

    if (last->length > 0) {
        process_extent(w, offset, last);
        offset += last->length;
    }

    free(extents);
}

static void *worker_thread(void *arg)
{
    struct worker *w = (struct worker *)arg;
    struct job *job = w->job;
    struct options *opt = job->opt;
    ssize_t i;

    DEBUG("worker %d started", w->id);

    w->s = open_src(job->src->uri);

    w->h = blkhash_new(opt->block_size, opt->digest_name);
    if (w->h == NULL)
        FAIL_ERRNO("blkhash_new");

    w->buf = malloc(opt->read_size);
    if (w->buf == NULL)
        FAIL_ERRNO("malloc");

    while ((i = next_segment(job)) != -1) {
        int64_t offset = i * opt->segment_size;
        unsigned char *seg_md = segment_md(job, i);

        DEBUG("worker %d processing segemnt %zd offset %" PRIi64,
              w->id, i, offset);

        process_segment(w, offset);
        blkhash_final(w->h, seg_md, NULL);
        blkhash_reset(w->h);
    }

    free(w->buf);
    blkhash_free(w->h);
    src_close(w->s);

    DEBUG("worker %d finished", w->id);
    return NULL;
}

void parallel_checksum(const char *filename, struct options *opt,
                       unsigned char *out)
{
    struct job job;
    struct worker *workers;
    size_t worker_count;
    int err;

    init_job(&job, filename, opt);

    worker_count = (job.segment_count < opt->max_workers)
        ? job.segment_count : opt->max_workers;

    workers = calloc(worker_count, sizeof(*workers));
    if (workers == NULL)
        FAIL_ERRNO("calloc");

    for (int i = 0; i < worker_count; i++) {
        struct worker *w = &workers[i];
        w->id = i;
        w->job = &job;
        DEBUG("starting worker %d", i);
        err = pthread_create(&w->thread, NULL, worker_thread, w);
        if (err)
            FAIL("pthread_create: %s", strerror(err));
    }

    for (int i = 0; i < worker_count; i++) {
        DEBUG("joining worker %d", i);
        err = pthread_join(workers[i].thread, NULL);
        if (err)
            FAIL("pthread_join: %s", strerror(err));
    }

    compute_root_hash(&job, out);

    destroy_job(&job);
    free(workers);
}
