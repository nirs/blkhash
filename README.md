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

First we split the image to segments of 128 MiB. If the image is not
aligned to 128 MiB, the last segment will be smaller. Using segments
makes it easy to use multiple threads to compute checksum in parallel.

To compute checksum for segment, we split the segment to blocks of equal
size. If the image is not aligned to block size, the last block of the
last segment will be shorter.

For every block, we detect if the block contains only null bytes, and
reuse a precomputed zero block digest instead of computing the value.
Detecting zero block is about order of magnitude faster than computing a
checksum using fast algorithm such as sha1.

When accessing an image via qemu-nbd, we can detect zero areas without
reading any data, minimizing I/O and cpu usage.

The hash is computed using:

    H( H(segment 1) + H(segment 2) ... H(segment N) )

Where H(segment N) is:

    H( H(block 1) + H(block 2) ... H(block M) )

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

To compute a checksum of qcow2 image export the image using qemu-nbd and
specify a NBD URL instead of path to the image:

    $ qemu-nbd --read-only --persistent --shared=4 --format=qcow2 fedora-32.qcow2

    $ blksum sha1 nbd://localhost
    1edf578c3c17322557208f85ddad67d8f0e129a8  nbd://localhost

Note that --shared=4 is required to allow blksum to open 4 connections
to qemu-nbd.

If the image is on the same host, using unix socket avoids managing
ports:

    $ qemu-nbd --read-only --persistent --shared=4 --format=qcow2 \
        --socket=/tmp/nbd.sock fedora-32.qcow2

    $ blksum sha1 nbd+unix:///?socket=/tmp/nbd.sock
    1edf578c3c17322557208f85ddad67d8f0e129a8  nbd+unix:///?socket=/tmp/nbd.sock

Using NBD URL is faster in most cases, since blksum can detect image
extents without reading the entire image. However for fully allocated
image accessing the image directly is faster.

## Benchmarks

Here are some examples comparing blksum to sha1sum.
The images are access both directly and via qemu-nbd.

Fedora 32 raw and qcow2 images created with virt-builder:

    $ ls -lhs fedora-32.*
    1.6G -rw-r--r--. 1 nsoffer nsoffer 1.6G Jan 28 01:01 fedora-32.qcow2
    1.6G -rw-r--r--. 1 nsoffer nsoffer 6.0G Jan 30 23:37 fedora-32.raw

    $ hyperfine -w3 "build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock" \
                    "build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock" \
                    "build/blksum sha1 /var/tmp/fedora-32.raw" \
                    "build/blksum sha1 < /var/tmp/fedora-32.raw" \
                    "sha1sum /var/tmp/fedora-32.raw"
    Benchmark #1: build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock
      Time (mean ± σ):     888.8 ms ±  62.0 ms    [User: 2.379 s, System: 0.264 s]
      Range (min … max):   789.6 ms … 1006.6 ms    10 runs

    Benchmark #2: build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock
      Time (mean ± σ):     892.3 ms ±  55.5 ms    [User: 2.371 s, System: 0.258 s]
      Range (min … max):   842.1 ms … 996.0 ms    10 runs

    Benchmark #3: build/blksum sha1 /var/tmp/fedora-32.raw
      Time (mean ± σ):     924.0 ms ±   5.3 ms    [User: 2.509 s, System: 1.124 s]
      Range (min … max):   917.2 ms … 930.5 ms    10 runs

    Benchmark #4: build/blksum sha1 < /var/tmp/fedora-32.raw
      Time (mean ± σ):      2.843 s ±  0.050 s    [User: 2.005 s, System: 0.833 s]
      Range (min … max):    2.755 s …  2.910 s    10 runs

    Benchmark #5: sha1sum /var/tmp/fedora-32.raw
      Time (mean ± σ):      8.062 s ±  0.080 s    [User: 7.044 s, System: 1.000 s]
      Range (min … max):    7.923 s …  8.171 s    10 runs

    Summary
      'build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock' ran
        1.00 ± 0.09 times faster than 'build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock'
        1.04 ± 0.07 times faster than 'build/blksum sha1 /var/tmp/fedora-32.raw'
        3.20 ± 0.23 times faster than 'build/blksum sha1 < /var/tmp/fedora-32.raw'
        9.07 ± 0.64 times faster than 'sha1sum /var/tmp/fedora-32.raw'

Fully allocated image full of zeroes, created with dd:

    $ dd if=/dev/zero bs=8M count=768 of=zero-6g.raw

    $ ls -lhs zero-6g.raw
    6.1G -rw-rw-r--. 1 nsoffer nsoffer 6.0G Feb 12 21:57 zero-6g.raw

    $ hyperfine -w3 "build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock" \
                    "build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock" \
                    "build/blksum sha1 /var/tmp/zero-6g.raw" \
                    "build/blksum sha1 < /var/tmp/zero-6g.raw" \
                    "sha1sum /var/tmp/zero-6g.raw"
    Benchmark #1: build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock
      Time (mean ± σ):      2.297 s ±  0.016 s    [User: 872.6 ms, System: 1364.3 ms]
      Range (min … max):    2.284 s …  2.337 s    10 runs

    Benchmark #2: build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock
      Time (mean ± σ):      2.485 s ±  0.013 s    [User: 871.2 ms, System: 1434.2 ms]
      Range (min … max):    2.469 s …  2.517 s    10 runs

    Benchmark #3: build/blksum sha1 /var/tmp/zero-6g.raw
      Time (mean ± σ):     789.7 ms ±  10.4 ms    [User: 757.5 ms, System: 2329.3 ms]
      Range (min … max):   777.4 ms … 809.4 ms    10 runs

    Benchmark #4: build/blksum sha1 < /var/tmp/zero-6g.raw
      Time (mean ± σ):      1.084 s ±  0.004 s    [User: 244.5 ms, System: 835.8 ms]
      Range (min … max):    1.079 s …  1.094 s    10 runs

    Benchmark #5: sha1sum /var/tmp/zero-6g.raw
      Time (mean ± σ):      7.988 s ±  0.101 s    [User: 7.015 s, System: 0.958 s]
      Range (min … max):    7.814 s …  8.103 s    10 runs

    Summary
      'build/blksum sha1 /var/tmp/zero-6g.raw' ran
        1.37 ± 0.02 times faster than 'build/blksum sha1 < /var/tmp/zero-6g.raw'
        2.91 ± 0.04 times faster than 'build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock'
        3.15 ± 0.04 times faster than 'build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock'
       10.12 ± 0.18 times faster than 'sha1sum /var/tmp/zero-6g.raw'

Empty image using raw and qcow2 format:

    $ hyperfine -w3 "build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock" \
                    "build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock" \
                    "build/blksum sha1 /var/tmp/empty-6g.raw" \
                    "build/blksum sha1 < /var/tmp/empty-6g.raw" \
                    "sha1sum /var/tmp/empty-6g.raw"
    Benchmark #1: build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock
      Time (mean ± σ):       5.4 ms ±   0.3 ms    [User: 7.2 ms, System: 2.7 ms]
      Range (min … max):     4.7 ms …   7.1 ms    420 runs

    Benchmark #2: build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock
      Time (mean ± σ):       5.5 ms ±   0.3 ms    [User: 7.5 ms, System: 2.7 ms]
      Range (min … max):     4.7 ms …   6.9 ms    418 runs

    Benchmark #3: build/blksum sha1 /var/tmp/empty-6g.raw
      Time (mean ± σ):     740.3 ms ±  16.4 ms    [User: 775.7 ms, System: 2123.2 ms]
      Range (min … max):   717.6 ms … 773.6 ms    10 runs

    Benchmark #4: build/blksum sha1 < /var/tmp/empty-6g.raw
      Time (mean ± σ):     995.1 ms ±   9.5 ms    [User: 243.1 ms, System: 749.5 ms]
      Range (min … max):   989.2 ms … 1012.6 ms    10 runs

    Benchmark #5: sha1sum /var/tmp/empty-6g.raw
      Time (mean ± σ):      7.595 s ±  0.092 s    [User: 6.704 s, System: 0.880 s]
      Range (min … max):    7.451 s …  7.736 s    10 runs

    Summary
      'build/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock' ran
        1.02 ± 0.08 times faster than 'build/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock'
      136.78 ± 8.38 times faster than 'build/blksum sha1 /var/tmp/empty-6g.raw'
      183.86 ± 10.65 times faster than 'build/blksum sha1 < /var/tmp/empty-6g.raw'
     1403.40 ± 81.93 times faster than 'sha1sum /var/tmp/empty-6g.raw'

## Portability

blkhash it developed and tested only on Linux, but it should be portable
to other platforms where openssl works.

## Setting up development environment

Install packages (for Fedora):

    dnf install \
        asciidoc \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        python3

Get the source:

    git clone https://gitlab.com/nirs/blkhash.git

Configure and update submodules:

    git submodule init
    git submodule update

To run the tests, you need to setup a python environment:

    python3 -m venv ~/venv/blkhash
    source ~/venv/blkhash/bin/activate
    pip install pytest
    deactivate

## Building

To build debug version run:

    meson setup build
    meson compile -C build

If "meson compile" does not work, you probably have old meson (< 0.55)
and need to run:

    ninja-build -C build

To build release version:

    meson setup build --buildtype release --prefix /usr
    meson compile -C build

To install use:

    sudo meson install -C build

Instead of specifying the directory, you can run the command inside the
build directory:

    cd build
    meson compile

## Debugging

To view debug logs run with:

    BLKSUM_DEBUG=1 blksum sha1 disk.img

## Running the tests

To run the tests, you need to enter the virtual environment:

    source ~/venv/blkhash/bin/activate

When you are done, you can exit from the virtual environment:

    deactivate

To run all tests:

    meson test -C build

Instead of specifying the directory, you can run the command inside the
build directory:

    cd build
    meson test

To see verbose test output use:

    meson test -C build -v

To run specific blksum tests, use pytest directly:

    meson -C build compile
    pytest -v -k sha1-sparse

pytest uses the "build" directory by default. If you want to use another
directory name, or installed blksum executable, specify the path to the
executable in the environment:

    meson setup release --buildtype release
    meson compile -C release
    BLKSUM=release/blksum pytest

To run only blkhash tests:

   meson compile -C build
   build/blkhash_test

## License

blkhash licensed under the GNU Lesser General Public License version 2.1.
See the file `LICENCE` for details.
