<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# blkhash

Block based hash optimized for disk images.

![blksum demo](https://i.imgur.com/BYAo5Ei.gif)

Disk images are typically sparse, containing unallocated areas read as
zeroes by the guest. The `blkhash` hash algorithm is optimized for
computing checksums of sparse disk images.

This project provides the `blkahsh` C library, and the `blksum` command.

## The blksum command

The `blksum` command computes message digest for disk images, similar to
standard tools like `sha1sum`. For example to compute a sha1 block based
checksum of a raw image:

    $ blksum sha1 fedora-35.raw
    f4ee0c53d0c7346c9ef7abcddfff386c59dd53fd  fedora-35.raw

We could compute a checksum for this image using `sha1sum`:

    $ sha1sum fedora-35.raw
    dfe8453ccd31857078ad8a3a1d2e08d87ef6db43  fedora-35.raw

Note that the checksums are different! `blksum` computes a block based
sha1 checksum, not a sha1 checksum of the entire image.

See [blksum(1)](blksum.1.adoc) for the manual.

### Supported image formats

The advantage of the block based checksum is clear when you want to
compute the checksum of the same image in different image format. Let's
convert this image to qcow2 format:

    $ qemu-img convert -f raw -O qcow2 fedora-35.raw fedora-35.qcow2

This creates and identical image:

    $ qemu-img compare fedora-35.raw fedora-35.qcow2
    Images are identical.

But the file contents are different:

    $ ls -lhs fedora-35.*
    1.2G -rw-r--r--. 1 nsoffer nsoffer 1.2G Feb 27 12:48 fedora-35.qcow2
    1.2G -rw-r--r--. 1 nsoffer nsoffer 6.0G Jan 21 00:28 fedora-35.raw

Standard tools like `sha1sum` do not understand image formats, so they
compute a different checksum for the qcow2 image:

    $ sha1sum fedora-35.qcow2
    1c2877aa89abf633e18bc02f22a2c32eb8942282  fedora-35.qcow2

Because `blksum` understands image formats, it compute the same checksum:

    $ blksum sha1 fedora-35.qcow2
    f4ee0c53d0c7346c9ef7abcddfff386c59dd53fd  fedora-35.qcow2

Currently only `raw` and `qcow2` formats are supported. Any other format
is considered a raw image.

### Supported digest algorithms

The `blksum` command supports any message digest algorithm provided by
`openssl`. For example we can use `blake2b512`:

    $ blksum blake2b512 fedora-35.qcow2
    fd2b46c3d5684fff9e1347299ef48d1a5c48cf3ec8ccf409112d6cd20e53874b7b2ab0c3a85d22e1cb63682796ecfa7687224131cf5d64c3e1e715c8e2848c34  fedora-35.qcow2

To find the available digest names you can use:

    $ openssl dgst -list
    Supported digests:
    -blake2b512                -blake2s256                -md4
    -md5                       -md5-sha1                  -ripemd
    -ripemd160                 -rmd160                    -sha1
    -sha224                    -sha256                    -sha3-224
    -sha3-256                  -sha3-384                  -sha3-512
    -sha384                    -sha512                    -sha512-224
    -sha512-256                -shake128                  -shake256
    -sm3                       -ssl3-md5                  -ssl3-sha1
    -whirlpool

### Supported sources

The `blksum` command can compute a checksum for a file, block device, NBD
URI, or pipe.

For example, lets create a logical volume with the contents of the qcow2
image:

    $ sudo lvs -o vg_name,lv_name,size,attr data/test
      VG   LV   LSize Attr
      data test 6.00g -wi-a-----

    $ sudo qemu-img convert -f qcow2 -O qcow2 fedora-35.qcow2 /dev/data/test

The `blksum` command will compute the same checksum for the logical volume:

    $ sudo blksum sha1 /dev/data/test
    f4ee0c53d0c7346c9ef7abcddfff386c59dd53fd  /dev/data/test

The `blksum` command can also use a NBD URI, accessing an image exported
by NBD server such as `qemu-nbd`.

For example we can export an image using `qemu-nbd` using a unix socket:

    $ qemu-nbd --read-only --persistent --shared 8 --socket /tmp/nbd.sock \
        --format qcow2 fedora-35.qcow2 &

    $ blksum sha1 nbd+unix:///?socket=/tmp/nbd.sock
    f4ee0c53d0c7346c9ef7abcddfff386c59dd53fd  nbd+unix:///?socket=/tmp/nbd.sock

We can also access an image on a remote host using NBD TCP URI:

    $ qemu-nbd --read-only --persistent --shared 8 --format qcow2 fedora-35.qcow2 &

    $ blksum sha1 nbd://localhost
    f4ee0c53d0c7346c9ef7abcddfff386c59dd53fd  nbd://localhost

The `blksum` command does not support yet secure NBD connections, so its
use for accessing images on remote hosts is limited.

Finally, we can also compute a checksum for data written to a pipe:

    $ cat fedora-35.raw | blksum sha1
    f4ee0c53d0c7346c9ef7abcddfff386c59dd53fd  -

Using a pipe we can only use `raw` format, and computing the checksum is
much less efficient.

### blksum performance

The `blksum` command optimizes checksum computation using sparseness
detection, zero detection, and multi-threading.

For typical sparse images, `blksum` can be 10 times faster compared with
standard tools:

```
$ hyperfine -w1 "blksum sha1 fedora-35.raw" "sha1sum fedora-35.raw"
Benchmark 1: blksum sha1 fedora-35.raw
  Time (mean ± σ):     515.5 ms ±  15.1 ms    [User: 1082.1 ms, System: 628.0 ms]
  Range (min … max):   496.7 ms … 543.0 ms    10 runs

Benchmark 2: sha1sum fedora-35.raw
  Time (mean ± σ):      5.591 s ±  0.113 s    [User: 4.907 s, System: 0.669 s]
  Range (min … max):    5.465 s …  5.797 s    10 runs

Summary
  'blksum sha1 fedora-35.raw' ran
   10.85 ± 0.39 times faster than 'sha1sum fedora-35.raw'
```

For images with more data, the difference is smaller. In this example,
the fedora-35-data.raw contains additional 3 GiB of random data, and
is 65% full.

```
$ hyperfine -w1 "blksum sha1 fedora-35-data.raw" "sha1sum fedora-35-data.raw"
Benchmark 1: blksum sha1 fedora-35-data.raw
  Time (mean ± σ):      1.550 s ±  0.021 s    [User: 3.924 s, System: 2.083 s]
  Range (min … max):    1.513 s …  1.575 s    10 runs

Benchmark 2: sha1sum fedora-35-data.raw
  Time (mean ± σ):      5.777 s ±  0.199 s    [User: 5.064 s, System: 0.696 s]
  Range (min … max):    5.443 s …  6.122 s    10 runs

Summary
  'blksum sha1 fedora-35-data.raw' ran
    3.73 ± 0.14 times faster than 'sha1sum fedora-35-data.raw'
```

The best case is a completely empty image. Here is an example computing
a checksum for a 6 GiB empty image:

```
$ hyperfine -w1 "blksum sha1 empty-6g.raw" "sha1sum empty-6g.raw"
Benchmark 1: blksum sha1 empty-6g.raw
  Time (mean ± σ):      32.6 ms ±   4.7 ms    [User: 6.6 ms, System: 4.7 ms]
  Range (min … max):    24.1 ms …  47.4 ms    76 runs

Benchmark 2: sha1sum empty-6g.raw
  Time (mean ± σ):      5.572 s ±  0.096 s    [User: 4.883 s, System: 0.675 s]
  Range (min … max):    5.471 s …  5.735 s    10 runs

Summary
  'blksum sha1 empty-6g.raw' ran
  170.81 ± 24.56 times faster than 'sha1sum empty-6g.raw'
```

When reading from a pipe we cannot use multi-threading or sparseness
detection, but zero detection is effective:

```
$ hyperfine -w1 "blksum sha1 <fedora-35.raw" "sha1sum fedora-35.raw"
Benchmark 1: blksum sha1 <fedora-35.raw
  Time (mean ± σ):      1.604 s ±  0.047 s    [User: 1.016 s, System: 0.576 s]
  Range (min … max):    1.551 s …  1.707 s    10 runs

Benchmark 2: sha1sum fedora-35.raw
  Time (mean ± σ):      5.667 s ±  0.103 s    [User: 4.960 s, System: 0.692 s]
  Range (min … max):    5.518 s …  5.774 s    10 runs

Summary
  'blksum sha1 <fedora-35.raw' ran
    3.53 ± 0.12 times faster than 'sha1sum fedora-35.raw'
```

The worst case is a completely full image, when nothing can be
optimized, but using multi-threading helps:

```
$ hyperfine -w1 "blksum sha1 full-6g.raw" "sha1sum full-6g.raw"
Benchmark 1: blksum sha1 full-6g.raw
  Time (mean ± σ):      2.371 s ±  0.077 s    [User: 5.906 s, System: 3.243 s]
  Range (min … max):    2.280 s …  2.480 s    10 runs

Benchmark 2: sha1sum full-6g.raw
  Time (mean ± σ):      5.753 s ±  0.139 s    [User: 5.002 s, System: 0.731 s]
  Range (min … max):    5.522 s …  6.044 s    10 runs

Summary
  'blksum sha1 full-6g.raw' ran
    2.43 ± 0.10 times faster than 'sha1sum full-6g.raw'
```

## The blkhash library

The `blkhash` C library implements the block based hash algorithm and zero
detection.

The library provides the expected interface for creating, updating,
finalizing and destroying a hash. The `blkhash_update()` function
implements zero detection, speeding up data processing. When you know
that some areas of the image are read as zeroes, you can skip reading
them and use `blkhash_zero()` to add zero range to the hash.

See the [example program](example.c) for example of using the library.

See [blkhash(3)](blkhash.3.adoc) for complete documentation.

## Installing

The project is not packaged yet, so the only way to use this project now
is to build and install from source.

## Portability

The `blkhash` library and `blksum` command are devolped on Linux, but
should be portable to any platform where openssl is available, but some
optimizations are implemented only for linux.

The `blksum` command requires
[libnbd](https://libguestfs.org/libnbd.3.html) for NBD support, and
[qemu-nbd](https://www.qemu.org/docs/master/tools/qemu-nbd.html) for
`qcow2` format support. If `libdnb` is not available, `blksum` is built
without NBD support and can be used only with `raw` images.

### Testing status

| Arch      | OS                | CI    | libnbd |
|-----------|-------------------|-------|--------|
| x86_64    | Fedora 35         | yes   | yes    |
| x86_64    | CentOS Stream 8   | yes   | yes    |
| x86_64    | CentOS Stream 9   | yes   | yes    |
| x86_64    | RHEL 8.6          | no    | yes    |
| x86_64    | FreeBSD 13        | no    | no     |
| Apple M1  | macOS 11 Big Sur  | no    | no     |

## Contributing

Contribution is welcome!

Please check issues with the
[help needed label](https://gitlab.com/nirs/blkhash/-/issues?sort=priority&state=opened&label_name[]=help+needed)
for issues that need your help.

Please [create a new issue](https://gitlab.com/nirs/blkhash/-/issues/new)
for new features you want to work on.

For discussing development, please use the
[oVirt devel mailing list](https://lists.ovirt.org/archives/list/devel%40ovirt.org/)
or the `#ovirt` room in `irc.oftc.net`.

See the next sections on how to set up a development environment, run
the tests and debug.

### Setting up development environment

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

macOS 11 Big Sur:

    # Requires macport from https://www.macports.org/install.php
    port install pkgconfig openssl asciidoc meson
    port select --set python3 python39

Get the source:

    git clone https://gitlab.com/nirs/blkhash.git

Configure and update submodules:

    git submodule init
    git submodule update

To run the tests, you need to setup a python3 environment:

    python3 -m venv ~/venv/blkhash
    source ~/venv/blkhash/bin/activate
    pip install pytest
    deactivate

On FreeBSD using the default shell (sh), use "." instead of "source":

    . ~/venv/blkhash/bin/activate

### Configuring

Create a build directory with default options:

    meson setup build

The default options:

- nbd=auto - Support NBD if libnbd is available.

To configure build directory for release installing in /usr:

    meson configure build --buildtype=release --prefix=/usr

To see all available options and possible values:

    meson configure build

### Building

To build run:

    meson compile -C build

If "meson compile" does not work, you probably have old meson (< 0.55)
and need to run:

    ninja-build -C build

Instead of specifying the directory, you can run the command inside the
build directory:

    cd build
    meson compile

### Installing

To install at configured --prefix:

    meson install -C build

### Debugging

To view debug logs run with:

    BLKSUM_DEBUG=1 blksum sha1 disk.img

To skip checksum calculation for testing I/O performance:

    BLKSUM_IO_ONLY=1 blksum sha1 disk.img

### Running the tests

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

To run specific `blksum` tests, use pytest directly:

    meson -C build compile
    pytest -k sha1-sparse

If `blksum` is built with NBD support, enable the NBD tests:

    HAVE_NBD=1 pytest -k sha1-sparse

pytest uses the "build" directory by default. If you want to use another
directory name, or installed `blksum` executable, specify the path to the
executable in the environment:

    meson setup release --buildtype=release
    meson compile -C release
    BLKSUM=release/blksum pytest

To run only `blkhash` tests:

   meson compile -C build
   build/blkhash_test


## Related projects

- The `blkhash` algorithm is based on
  [ovirt-imageio](https://github.com/oVirt/ovirt-imageio)
  [blkhash module](https://github.com/oVirt/ovirt-imageio/blob/master/ovirt_imageio/_internal/blkhash.py).

- The `blksum` command NBD support is powered by the
  [libnbd](https://gitlab.com/nbdkit/libnbd/) library, and the
  implementaion is inspired by the
  [nbdcopy](https://gitlab.com/nbdkit/libnbd/-/tree/master/copy) command.

## License

blkhash is licensed under the GNU Lesser General Public License version
2.1 or later. See the file `LICENSES/LGPL-2.1-or-later.txt` for details.
