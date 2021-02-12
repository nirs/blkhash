# blkhash

Block based hash optimized for disk images.

Disk images are usually sparse - contain unallocated areas that read as
zeroes.  Computing a checksum of sparse image with standard tools is
slow as computing a checksum of fully allocated image.

blkhash is optimized for sparse disk images and disk images with with
zeroed areas. Instead of computing a digest for the entire image, it
computes a digest of every block, and created a image digest from the
block digests. When detecting a block containing only zeroes, it uses a
precomputed zero block digest.

## Computing a block hash

The hash is computed using:

    H( H(block 1) + H(block 2) ... H(block N) )

H can be any message digest algorithm provided by openssl.

## blksum

The blksum tool computes a checksum using the provided message digest
name.

Computing a checksum for an image:

    $ blksum sha1 disk.img
    9eeb7c21316d1e1569139ad36452b00eeaabb624  disk.img

Computing a checksum from stdin:

    $ blksum sha1 <disk.img
    9eeb7c21316d1e1569139ad36452b00eeaabb624  -

## Benchmarks

Here are some examples comparing blksum to sha1sum.

Fedora 32 raw image created with virt-builder:

    $ ls -lhs fedora-32.raw
    1.6G -rw-r--r--. 1 nsoffer nsoffer 6.0G Jan 30 23:37 fedora-32.raw

    $ hyperfine -w3 "./blksum sha1 fedora-32.raw" "sha1sum fedora-32.raw"
    Benchmark #1: ./blksum fedora-32.raw sha1
      Time (mean ± σ):      2.465 s ±  0.082 s    [User: 1.802 s, System: 0.656 s]
      Range (min … max):    2.287 s …  2.590 s    10 runs

    Benchmark #2: sha1sum fedora-32.raw
      Time (mean ± σ):      7.456 s ±  0.094 s    [User: 6.521 s, System: 0.918 s]
      Range (min … max):    7.326 s …  7.603 s    10 runs

    Summary
      './blksum sha1 fedora-32.raw' ran
        3.03 ± 0.11 times faster than 'sha1sum fedora-32.raw'

Fully allocated image full of zeroes, created with dd:

    $ dd if=/dev/zero bs=8M count=768 of=zero-6g.raw

    $ ls -lhs zero-6g.raw
    6.1G -rw-rw-r--. 1 nsoffer nsoffer 6.0G Feb 12 21:57 zero-6g.raw

    $ hyperfine -w3 "./blksum sha1 zero-6g.raw" "sha1sum zero-6g.raw"
    Benchmark #1: ./blksum sha1 zero-6g.raw
      Time (mean ± σ):     798.1 ms ±  23.8 ms    [User: 152.5 ms, System: 643.8 ms]
      Range (min … max):   779.3 ms … 851.5 ms    10 runs

    Benchmark #2: sha1sum zero-6g.raw
      Time (mean ± σ):      7.539 s ±  0.164 s    [User: 6.513 s, System: 1.009 s]
      Range (min … max):    7.205 s …  7.709 s    10 runs

    Summary
      './blksum sha1 zero-6g.raw' ran
        9.45 ± 0.35 times faster than 'sha1sum zero-6g.raw'

## Image formats

Currently only raw images are supported. Supporting other format is
planned via qemu-nbd.

## Portability

blkhash it developed and tested only on Linux, but it should be portable
to other platforms where openssl works.

## License

blkhash licensed under the GNU Lesser General Public License version 2.1.
See the file `LICENCE` for details.
