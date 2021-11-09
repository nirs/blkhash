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

    $ hyperfine -w3 "blksum sha1 fedora-32.qcow2" \
                    "blksum sha1 fedora-32.raw" \
                    "blksum sha1 <fedora-32.raw" \
                    "sha1sum fedora-32.raw"
    Benchmark 1: blksum sha1 fedora-32.qcow2
      Time (mean ± σ):     974.7 ms ±  66.1 ms    [User: 2.254 s, System: 1.467 s]
      Range (min … max):   860.2 ms … 1083.5 ms    10 runs

    Benchmark 2: blksum sha1 fedora-32.raw
      Time (mean ± σ):      1.078 s ±  0.042 s    [User: 2.407 s, System: 1.677 s]
      Range (min … max):    0.985 s …  1.118 s    10 runs

    Benchmark 3: blksum sha1 <fedora-32.raw
      Time (mean ± σ):      2.439 s ±  0.028 s    [User: 1.658 s, System: 0.771 s]
      Range (min … max):    2.399 s …  2.495 s    10 runs

    Benchmark 4: sha1sum fedora-32.raw
      Time (mean ± σ):      6.825 s ±  0.128 s    [User: 5.833 s, System: 0.974 s]
      Range (min … max):    6.696 s …  6.986 s    10 runs

    Summary
      'blksum sha1 fedora-32.qcow2' ran
        1.11 ± 0.09 times faster than 'blksum sha1 fedora-32.raw'
        2.50 ± 0.17 times faster than 'blksum sha1 <fedora-32.raw'
        7.00 ± 0.49 times faster than 'sha1sum fedora-32.raw'

Fully allocated image full of zeroes, created with dd:

    $ dd if=/dev/zero bs=8M count=768 of=zero-6g.raw

    $ ls -lhs zero-6g.raw
    6.1G -rw-rw-r--. 1 nsoffer nsoffer 6.0G Feb 12 21:57 zero-6g.raw

    $ hyperfine -w3 "blksum sha1 zero-6g.qcow2" \
                    "blksum sha1 zero-6g.raw" \
                    "blksum sha1 <zero-6g.raw" \
                    "sha1sum zero-6g.raw"
    Benchmark 1: blksum sha1 zero-6g.qcow2
      Time (mean ± σ):      2.629 s ±  0.039 s    [User: 1.050 s, System: 7.252 s]
      Range (min … max):    2.566 s …  2.669 s    10 runs

    Benchmark 2: blksum sha1 zero-6g.raw
      Time (mean ± σ):      2.742 s ±  0.028 s    [User: 1.094 s, System: 7.646 s]
      Range (min … max):    2.703 s …  2.796 s    10 runs

    Benchmark 3: blksum sha1 <zero-6g.raw
      Time (mean ± σ):     906.6 ms ±  18.5 ms    [User: 155.2 ms, System: 747.0 ms]
      Range (min … max):   888.5 ms … 940.3 ms    10 runs

    Benchmark 4: sha1sum zero-6g.raw
      Time (mean ± σ):      6.916 s ±  0.126 s    [User: 5.868 s, System: 1.029 s]
      Range (min … max):    6.763 s …  7.086 s    10 runs

    Summary
      'blksum sha1 <zero-6g.raw' ran
        2.90 ± 0.07 times faster than 'blksum sha1 zero-6g.qcow2'
        3.02 ± 0.07 times faster than 'blksum sha1 zero-6g.raw'
        7.63 ± 0.21 times faster than 'sha1sum zero-6g.raw'

Empty image using raw and qcow2 format:

    $ hyperfine -w3 "blksum sha1 empty.qcow2" \
                    "blksum sha1 empty.raw" \
                    "blksum sha1 <empty.raw" \
                    "sha1sum empty.raw"
    Benchmark 1: blksum sha1 empty.qcow2
      Time (mean ± σ):       9.5 ms ±   0.7 ms    [User: 9.5 ms, System: 6.2 ms]
      Range (min … max):     8.3 ms …  12.3 ms    261 runs

    Benchmark 2: blksum sha1 empty.raw
      Time (mean ± σ):       9.2 ms ±   0.8 ms    [User: 9.4 ms, System: 6.0 ms]
      Range (min … max):     8.2 ms …  13.7 ms    268 runs

    Benchmark 3: blksum sha1 <empty.raw
      Time (mean ± σ):     877.5 ms ±   9.9 ms    [User: 158.2 ms, System: 714.3 ms]
      Range (min … max):   867.2 ms … 902.1 ms    10 runs

    Benchmark 4: sha1sum empty.raw
      Time (mean ± σ):      6.641 s ±  0.048 s    [User: 5.608 s, System: 1.009 s]
      Range (min … max):    6.536 s …  6.690 s    10 runs

    Summary
      'blksum sha1 empty.raw' ran
        1.03 ± 0.11 times faster than 'blksum sha1 empty.qcow2'
       95.50 ± 8.08 times faster than 'blksum sha1 <empty.raw'
      722.79 ± 60.87 times faster than 'sha1sum empty.raw'

## Command line options

### -w, --workers

Set the number of workers. The default (4) gives good performance on 8
core laptop. On larger machines you may want to use more workers. The
valid value is 1 to the number of online cpus.

Here is example showing how number of core affects performance on 8 core
laptop.

    $ hyperfine -w1 -r3 -L workers 1,2,4,6,8 "blksum sha1 -w {workers} fedora-32.qcow2"
    Benchmark 1: blksum sha1 -w 1 fedora-32.qcow2
      Time (mean ± σ):      2.363 s ±  0.089 s    [User: 1.631 s, System: 1.003 s]
      Range (min … max):    2.261 s …  2.416 s    3 runs

    Benchmark 2: blksum sha1 -w 2 fedora-32.qcow2
      Time (mean ± σ):      1.362 s ±  0.005 s    [User: 1.785 s, System: 1.138 s]
      Range (min … max):    1.357 s …  1.367 s    3 runs

    Benchmark 3: blksum sha1 -w 4 fedora-32.qcow2
      Time (mean ± σ):     949.7 ms ±  30.1 ms    [User: 2.212 s, System: 1.412 s]
      Range (min … max):   916.7 ms … 975.8 ms    3 runs

    Benchmark 4: blksum sha1 -w 6 fedora-32.qcow2
      Time (mean ± σ):     857.3 ms ±  73.6 ms    [User: 2.751 s, System: 1.601 s]
      Range (min … max):   798.3 ms … 939.8 ms    3 runs

    Benchmark 5: blksum sha1 -w 8 fedora-32.qcow2
      Time (mean ± σ):     825.1 ms ±  67.5 ms    [User: 2.883 s, System: 1.645 s]
      Range (min … max):   748.2 ms … 874.3 ms    3 runs

    Summary
      'blksum sha1 -w 8 fedora-32.qcow2' ran
        1.04 ± 0.12 times faster than 'blksum sha1 -w 6 fedora-32.qcow2'
        1.15 ± 0.10 times faster than 'blksum sha1 -w 4 fedora-32.qcow2'
        1.65 ± 0.14 times faster than 'blksum sha1 -w 2 fedora-32.qcow2'
        2.86 ± 0.26 times faster than 'blksum sha1 -w 1 fedora-32.qcow2'

## Portability

blkhash it developed on Linux, but it should be portable to other platforms
where openssl is avaialble.

NBD suppoort requires libnbd. If libdnb is not availble, blksum is built
without NBD support.

Tested on:
- Fedora 32
- FreeBSD 13 (without libnbd)

## Setting up development environment

Fedora:

    dnf install \
        asciidoc \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        python3 \
        qemu-img

FreeBSD:

    pkg install \
        asciidoc \
        git \
        meson \
        openssl \
        pkgconf \
        python3 \
        qemu-utils

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

On FreeBSD using the default shell (sh), use "." instead of "source":

    . ~/venv/blkhash/bin/activate

## Configuring

Create a build directory with default options:

    meson setup build

The default options:

- nbd=auto - Support NBD if libnbd is available.

To configure build directory for release installing in /usr:

    meson configure build --buildtype=release --prefix=/usr

To see all available options and possible values:

    meson configure build

## Building

To build run:

    meson compile -C build

If "meson compile" does not work, you probably have old meson (< 0.55)
and need to run:

    ninja-build -C build

Instead of specifying the directory, you can run the command inside the
build directory:

    cd build
    meson compile

## Installing

To install at configured --prefix:

    sudo meson install -C build

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
    pytest -k sha1-sparse

If blksum is built with NBD support, enable the NBD tests:

    HAVE_NBD=1 pytest -k sha1-sparse

pytest uses the "build" directory by default. If you want to use another
directory name, or installed blksum executable, specify the path to the
executable in the environment:

    meson setup release --buildtype=release
    meson compile -C release
    BLKSUM=release/blksum pytest

To run only blkhash tests:

   meson compile -C build
   build/blkhash_test

## License

blkhash licensed under the GNU Lesser General Public License version 2.1.
See the file `LICENCE` for details.
