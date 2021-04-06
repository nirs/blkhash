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

## Benchmarks

Here are some examples comparing blksum to sha1sum.

Fedora 32 raw image created with virt-builder:

    $ ls -lhs fedora-32.raw
    1.6G -rw-r--r--. 1 nsoffer nsoffer 6.0G Jan 30 23:37 fedora-32.raw

    $ hyperfine -w3 "./blksum sha1 fedora-32.raw" "sha1sum fedora-32.raw"
    Benchmark #1: ./blksum sha1 fedora-32.raw
      Time (mean ± σ):      2.446 s ±  0.035 s    [User: 1.638 s, System: 0.804 s]
      Range (min … max):    2.401 s …  2.523 s    10 runs

    Benchmark #2: sha1sum fedora-32.raw
      Time (mean ± σ):      7.639 s ±  0.079 s    [User: 6.393 s, System: 1.234 s]
      Range (min … max):    7.598 s …  7.860 s    10 runs

    Summary
      './blksum sha1 fedora-32.raw' ran
        3.12 ± 0.05 times faster than 'sha1sum fedora-32.raw'

Fully allocated image full of zeroes, created with dd:

    $ dd if=/dev/zero bs=8M count=768 of=zero-6g.raw

    $ ls -lhs zero-6g.raw
    6.1G -rw-rw-r--. 1 nsoffer nsoffer 6.0G Feb 12 21:57 zero-6g.raw

    $ hyperfine -w3 "./blksum sha1 zero-6g.raw" "sha1sum zero-6g.raw"
    Benchmark #1: ./blksum sha1 zero-6g.raw
      Time (mean ± σ):     754.1 ms ±  20.7 ms    [User: 148.3 ms, System: 604.1 ms]
      Range (min … max):   743.7 ms … 810.8 ms    10 runs

    Benchmark #2: sha1sum zero-6g.raw
      Time (mean ± σ):      7.353 s ±  0.026 s    [User: 6.399 s, System: 0.942 s]
      Range (min … max):    7.320 s …  7.401 s    10 runs

    Summary
      './blksum sha1 zero-6g.raw' ran
        9.75 ± 0.27 times faster than 'sha1sum zero-6g.raw'

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
