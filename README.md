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

To compute a checksum of qcow2 image export the image using qemu-nbd and
specify a NBD URL instead of path to the image:

    $ qemu-nbd --read-only --persistent --format=qcow2 fedora-32.qcow2

    $ blksum sha1 nbd://localhost
    1edf578c3c17322557208f85ddad67d8f0e129a8  nbd://localhost

If the image is on the same host, using unix socket avoids managing
ports:

    $ qemu-nbd --read-only --persistent --format=qcow2 \
        --socket=/tmp/nbd.sock fedora-32.qcow2

    $ blksum sha1 nbd+unix:///?socket=/tmp/nbd.sock
    1edf578c3c17322557208f85ddad67d8f0e129a8  nbd+unix:///?socket=/tmp/nbd.sock

Using NBD URL is faster in most cases, since blksum can detect image
extents without reading the entire image

## Benchmarks

Here are some examples comparing blksum to sha1sum.
The images are access both directly and via qemu-nbd.

Fedora 32 raw and qcow2 images created with virt-builder:

    $ ls -lhs fedora-32.*
    1.6G -rw-r--r--. 1 nsoffer nsoffer 1.6G Jan 28 01:01 fedora-32.qcow2
    1.6G -rw-r--r--. 1 nsoffer nsoffer 6.0G Jan 30 23:37 fedora-32.raw

    $ hyperfine -w3 "release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock" \
                    "release/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock" \
                    "release/blksum sha1 fedora-32.raw" \
                    "sha1sum fedora-32.raw"
    Benchmark #1: release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock
      Time (mean ± σ):      2.454 s ±  0.062 s    [User: 1.802 s, System: 0.225 s]
      Range (min … max):    2.346 s …  2.520 s    10 runs

    Benchmark #2: release/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock
      Time (mean ± σ):      2.354 s ±  0.023 s    [User: 1.756 s, System: 0.202 s]
      Range (min … max):    2.318 s …  2.385 s    10 runs

    Benchmark #3: release/blksum sha1 fedora-32.raw
      Time (mean ± σ):      2.617 s ±  0.032 s    [User: 1.912 s, System: 0.700 s]
      Range (min … max):    2.574 s …  2.663 s    10 runs

    Benchmark #4: sha1sum fedora-32.raw
      Time (mean ± σ):      7.158 s ±  0.079 s    [User: 6.390 s, System: 0.757 s]
      Range (min … max):    7.086 s …  7.366 s    10 runs

    Summary
      'release/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock' ran
        1.04 ± 0.03 times faster than 'release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock'
        1.11 ± 0.02 times faster than 'release/blksum sha1 fedora-32.raw'
        3.04 ± 0.04 times faster than 'sha1sum fedora-32.raw'

Fully allocated image full of zeroes, created with dd:

    $ dd if=/dev/zero bs=8M count=768 of=zero-6g.raw

    $ ls -lhs zero-6g.raw
    6.1G -rw-rw-r--. 1 nsoffer nsoffer 6.0G Feb 12 21:57 zero-6g.raw

    $ hyperfine -w3 "release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock" \
                    "release/blksum sha1 zero-6g.raw" \
                    "sha1sum zero-6g.raw"
    Benchmark #1: release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock
      Time (mean ± σ):      4.213 s ±  0.112 s    [User: 478.8 ms, System: 890.5 ms]
      Range (min … max):    4.054 s …  4.373 s    10 runs

    Benchmark #2: release/blksum sha1 zero-6g.raw
      Time (mean ± σ):      1.033 s ±  0.003 s    [User: 247.1 ms, System: 783.4 ms]
      Range (min … max):    1.030 s …  1.039 s    10 runs

    Benchmark #3: sha1sum zero-6g.raw
      Time (mean ± σ):      7.539 s ±  0.182 s    [User: 6.677 s, System: 0.847 s]
      Range (min … max):    7.400 s …  8.008 s    10 runs

    Summary
      'release/blksum sha1 zero-6g.raw' ran
        4.08 ± 0.11 times faster than 'release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock'
        7.29 ± 0.18 times faster than 'sha1sum zero-6g.raw'

Empty image using raw and qcow2 format:

    $ hyperfine -w3 "release/blksum sha1 empty-1g.raw" \
                    "release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock" \
                    "release/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock" \
                    "sha1sum empty-1g.raw"
    Benchmark #1: release/blksum sha1 empty-1g.raw
      Time (mean ± σ):     172.9 ms ±   1.3 ms    [User: 41.8 ms, System: 130.4 ms]
      Range (min … max):   169.0 ms … 174.4 ms    17 runs

    Benchmark #2: release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock
      Time (mean ± σ):       3.7 ms ±   0.4 ms    [User: 2.2 ms, System: 1.2 ms]
      Range (min … max):     3.2 ms …   5.8 ms    414 runs

    Benchmark #3: release/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock
      Time (mean ± σ):       3.5 ms ±   0.3 ms    [User: 2.0 ms, System: 1.2 ms]
      Range (min … max):     3.0 ms …   5.2 ms    495 runs

    Benchmark #4: sha1sum empty-1g.raw
      Time (mean ± σ):      1.267 s ±  0.022 s    [User: 1.123 s, System: 0.142 s]
      Range (min … max):    1.222 s …  1.288 s    10 runs

    Summary
      'release/blksum sha1 nbd+unix:///?socket=/tmp/qcow2.sock' ran
        1.06 ± 0.15 times faster than 'release/blksum sha1 nbd+unix:///?socket=/tmp/raw.sock'
       49.35 ± 4.68 times faster than 'release/blksum sha1 empty-1g.raw'
      361.73 ± 34.78 times faster than 'sha1sum empty-1g.raw'

## Portability

blkhash it developed and tested only on Linux, but it should be portable
to other platforms where openssl works.

## Setting up development environment

Install packages (for Fedora):

    dnf install \
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
