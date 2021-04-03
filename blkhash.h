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

#ifndef BLKHASH_H
#define BLKHASH_H

struct blkhash;

/*
 * Allocates and initialize a block hash for creating one or more
 * message digests. The hash buffers block_size bytes for zero
 * detection. The messge digest is created using the provided message
 * digest name.
 */
struct blkhash *blkhash_new(size_t block_size, const char *md_name);

/*
 * Hashes len bytes of data at buf into the hash h. This function can be
 * called several times on the same hash to hash additional data. For
 * best performance, len should be aligned to the block size specified
 * in blkhash_new().
 *
 * This function etects zero blocks in buf to optimize hashing, so you
 * don't need to do this yourself. However if you know that a byte range
 * contains only zeroes, call blkhash_zero() instead, which is much
 * faster.
 */
void blkhash_update(struct blkhash *h, const void *buf, size_t len);

/*
 * Hashes len bytes of zeores efficiently into the hash h. This function
 * can be called several times on the same hash to hash additional
 * zeroes. For best performance, len should be aligned to the block size
 * specified in blkhash_new().
 *
 * Should be used when you know that a range of bytes is read as zeroes.
 * Example use cases are a hole in a sparse file, or area in qcow2 image
 * that is a hole or reads as zeroes. If you don't the contents of the
 * data, use blkhash_update().
 */
void blkhash_zero(struct blkhash *h, size_t len);

/*
 * Finalize a hash and return a message digest. See blkhash_reset() if
 * you want to create a new digest.
 */
void blkhash_final(struct blkhash *h, unsigned char *md_value,
                   unsigned int *md_len);

/*
 * Reset the hash for computing a new message digest.
 */
void blkhash_reset(struct blkhash *h);

/*
 * Free resources allocated in blkhash_init().
 */
void blkhash_free(struct blkhash *h);

#endif /* BLKHASH_H */
