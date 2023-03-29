// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKHASH_H
#define BLKHASH_H

#include <openssl/evp.h> /* For EVP_* */

/* Maxmum length of md_value buffer for any digest name. */
#define BLKHASH_MAX_MD_SIZE EVP_MAX_MD_SIZE

struct blkhash;
struct blkhash_opts;

/*
 * Allocate and initialize a block hash for creating one message digest
 * using the default options. To create a hash with non-default options
 * see blkhash_new_opts().
 *
 * Return NULL and set errno on error.
 */
struct blkhash *blkhash_new(void);

/*
 * Allocate and initialize a block hash using the specified options.
 * Note that using non-default options can change the hash value and
 * reduce interoperability.
 *
 * Return NULL and set errno on error.
 */
struct blkhash *blkhash_new_opts(const struct blkhash_opts *opts);

/*
 * Hash len bytes of data at buf into the hash h. This function can be
 * called several times on the same hash to hash additional data. For
 * best performance, len should be aligned to the block size specified
 * in blkhash_new().
 *
 * This function detects zero blocks in buf to optimize hashing, so you
 * don't need to do this yourself. However if you know that a byte range
 * contains only zeros, call blkhash_zero() instead, which is much
 * faster.
 *
 * Return 0 on success and errno value on error. All future calls will
 * fail after the first error.
 */
int blkhash_update(struct blkhash *h, const void *buf, size_t len);

/*
 * Hash len bytes of zeros efficiently into the hash h. This function
 * can be called several times on the same hash to hash additional
 * zeros. For best performance, len should be aligned to the block size
 * specified in blkhash_new().
 *
 * Should be used when you know that a range of bytes is read as zeros.
 * Example use cases are a hole in a sparse file, or area in qcow2 image
 * that is a hole or reads as zeros. If you don't know the contents of
 * the data, use blkhash_update().
 *
 * Return 0 on success and errno value on error. All future calls will
 * fail after the first error.
 */
int blkhash_zero(struct blkhash *h, size_t len);

/*
 * Finalize a hash and store the message digest in md_value.  At most
 * BLKHASH_MAX_MD_SIZE bytes will be written.
 *
 * If md_len is not NULL, store the length of the digest in md_len.
 *
 * Return 0 on success and errno value on error. The contents of
 * md_value and md_len are undefined on error.
 */
int blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len);

/*
 * Free resources allocated in blkhash_new().
 */
void blkhash_free(struct blkhash *h);

/*
 * Allocate blkhash_opts for digest name and initialize with the
 * default options. See blkhash_opts_set_*() and blkhash_opts_get_*()
 * for acessing the options.  When done free the resources using
 * blkhash_opts_free().
 *
 * Return NULL and set errno on error.
 */
struct blkhash_opts *blkhash_opts_new(const char *digest_name);

/*
 * Set the hash block size. The size should be power of 2, and your
 * buffer passed to blkhash_upodate() should be a multiple of this size.
 *
 * Returns EINVAL if the value is invalid.
 */
int blkhash_opts_set_block_size(struct blkhash_opts *o, size_t block_size);

/*
 * Set the number of threads for computing block hashes. The defualt (4)
 * is good for most cases, but if you have very fast storage and big
 * machine, using more threads can speed up hash computation.
 *
 * Return EINVAL if the value is invalid.
 */
int blkhash_opts_set_threads(struct blkhash_opts *o, uint8_t threads);

/*
 * Return the digest name.
 */
const char *blkhash_opts_get_digest_name(struct blkhash_opts *o);

/*
 * Return the block size.
 */
size_t blkhash_opts_get_block_size(struct blkhash_opts *o);

/*
 * Return the number of threads.
 */
uint8_t blkhash_opts_get_threads(struct blkhash_opts *o);

/*
 * Free resource allocated in blkhash_opts_new().
 */
void blkhash_opts_free(struct blkhash_opts *o);

#endif /* BLKHASH_H */
