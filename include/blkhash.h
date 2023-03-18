// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKHASH_H
#define BLKHASH_H

#include <openssl/evp.h> /* For EVP_* */

/* Maxmum length of md_value buffer for any digest name. */
#define BLKHASH_MAX_MD_SIZE EVP_MAX_MD_SIZE

struct blkhash;

/*
 * Allocates and initialize a block hash for creating one message
 * digest.  The message digest is created using the provided message
 * digest name.  The hash buffers block_size bytes for zero detection.
 * The hash uses the specified number of threads to compute block hashes
 * in parallel.
 *
 * Return NULL and set errno on error.
 */
struct blkhash *blkhash_new(const char *md_name, size_t block_size,
                            unsigned threads);

/*
 * Hashes len bytes of data at buf into the hash h. This function can be
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
 * Hashes len bytes of zeros efficiently into the hash h. This function
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

#endif /* BLKHASH_H */
