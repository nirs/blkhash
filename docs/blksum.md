<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# The blksum command

The `blksum` command computes message digest for disk images, similar to
standard tools like `sha256sum`.

## blksum features

- Understands image format - compute a checksum of the guest visible data
- Much faster - up to 4 orders of magnitude faster compared with standard tools
- Support multiple sources - file, block device, pipe, NBD URL
- Configurable digest algorithms - using any digest algorithm from `openssl`

## Image format

The `blksum` command computes a checksum of the guest visible data, so
images with the same content but using different format or compression
have the same checksum.

Here we copy a raw disk image to qcow2 format, creating a new image with
identical content:

    $ qemu-img convert -f raw -O qcow2 disk.raw disk.qcow2

    $ qemu-img compare disk.raw disk.qcow2
    Images are identical.

Standard tools like `sha256sum` do not understand image formats, so they
compute different checksums:

    $ sha256sum disk.raw
    d99ddd179856892dc662b0048f20d2240612d6f54a4b8115767ede1f91f2854e  disk.raw

    $ sha256sum disk.qcow2
    379b622dbdfd383019bb5e922c5fee8d9581a583214aa06fd53d4aa8e1763247  disk.qcow2

The `blksum` command computes the same checksum since the images are identical:

    $ blksum disk.raw
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.raw

    $ blksum disk.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.qcow2

Currently only `raw` and `qcow2` formats are supported. Other formats
(e.g. `vmdk`) are considered as a raw image.

## Image compression

Some image formats like `qcow2` can use internal compression, so
identical images with same format can have different host data. The
`blksum` command computes a checksum of the decompressed guest visible
data so it will compute the same checksum.

Here we create 2 identical images with different compression options:

    $ qemu-img convert -f qcow2 -O qcow2 -c -o compression_type=zlib disk.qcow2 disk.zlib.qcow2
    $ qemu-img convert -f qcow2 -O qcow2 -c -o compression_type=zstd disk.qcow2 disk.zstd.qcow2

The images are identical:

    $ qemu-img compare disk.qcow2 disk.zlib.qcow2
    Images are identical.

    $ qemu-img compare disk.qcow2 disk.zstd.qcow2
    Images are identical.

`sha256sum` computes different checksums:

    $ sha256sum disk.qcow2
    379b622dbdfd383019bb5e922c5fee8d9581a583214aa06fd53d4aa8e1763247  disk.qcow2

    $ sha256sum disk.zlib.qcow2
    2337a5bbb779509a27cbde18dd6135a94aa82424acd38fd8d5c2607595847ba4  disk.zlib.qcow2

    $ sha256sum disk.zstd.qcow2
    9fc96ddadd3cb406e6dd23f12e270c42d2484b39034b7406a8806112f3dfb992  disk.zstd.qcow2

`blksum` computes the same checksum since the images are identical:

    $ blksum disk.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.qcow2

    $ blksum disk.zlib.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.zlib.qcow2

    $ blksum disk.zstd.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.zstd.qcow2

## Image host layout

Images formats like `qcow2` can store the guest data in different order
on the host. For example here we copy an image using the unordered
writes (`-W`) option:

    $ qemu-img convert -f qcow2 -O qcow2 -W disk.qcow2 disk.unordered.qcow2

The image are identical:

    $ qemu-img compare disk.qcow2 disk.unordered.qcow2
    Images are identical.

`sha256sum` computes different checksums:

    $ sha256sum disk.qcow2
    379b622dbdfd383019bb5e922c5fee8d9581a583214aa06fd53d4aa8e1763247  disk.qcow2

    $ sha256sum disk.unordered.qcow2
    df2e0d7ce6e2e568a6bfa3a078ec3365f939a51051d919f0898b5823cd6f8bcf  disk.unordered.qcow2

`blksum` computes the same checksum since the images are identical:

    $ blksum disk.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.qcow2

    $ blksum disk.unordered.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  disk.unordered.qcow2

## qcow2 snapshots

Some image formats like `qcow2` support external snapshots, using chain
of images. We can use `blksum` to compute a checksum of the entire chain
and verify snapshot operations.

Here we create a snapshot on top of a base image:

    $ qemu-img convert -f raw -O qcow2 disk.raw base.qcow2

    $ qemu-img create -f qcow2 -b base.qcow2 -F qcow2 snapshot.qcow2

`blksum` computes a checksum of the entire chain, including data in the
snapshot and the base image. Because we did not write anything to the
snapshot, the checksum is still the same:

    $ blksum snapshot.qcow2
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  snapshot.qcow2

After writing data to the snapshot, the checksum is different:

    $ qemu-io -f qcow2 -c 'write -P 85 512m 1m' snapshot.qcow2

    $ blksum snapshot.qcow2
    8b86a8f28640d14d7df686249dd21e1b4cf0db3b6a3d6afd5cc93967bd459fc9  snapshot.qcow2

If we commit the snapshot data into the base image, the image content
must not change - we can verify it with `blksum`:

    $ qemu-img commit -f qcow2 -b base.qcow2 snapshot.qcow2
    Image committed.

    $ blksum base.qcow2
    8b86a8f28640d14d7df686249dd21e1b4cf0db3b6a3d6afd5cc93967bd459fc9  base.qcow2

## Computing a checksum from a block device

The `blksum` command can compute a checksum for guest visible data
stored in a block device.

Here we copy contents of the qcow2 image to a 6 GiB logical volume:

    $ sudo lvs -o vg_name,lv_name,size,attr data/test
      VG   LV   LSize Attr
      data test 6.00g -wi-a-----

    $ sudo qemu-img convert -f qcow2 -O qcow2 disk.qcow2 /dev/data/test

This copies only 1.7 GiB of data to the block device, since the qcow2
image is mostly empty.

The `blksum` command will compute the checksum for guest visible data in
the block device, reading only the guest data:

    $ sudo blksum /dev/data/test
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  /dev/data/test

## Computing a checksum from NBD URL

The `blksum` command can also use a `NBD` URL, accessing an image exported
by `NBD` server such as `qemu-nbd`.

For example we can export an image using *qemu-nbd* using a unix socket:

    $ qemu-nbd --read-only --persistent --socket /tmp/nbd.sock \
        --format qcow2 disk.qcow2 &

    $ blksum nbd+unix:///?socket=/tmp/nbd.sock
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  nbd+unix:///?socket=/tmp/nbd.sock

We can also access an image on a remote host using `NBD` TCP URL:

    $ qemu-nbd --read-only --persistent --format qcow2 disk.qcow2 &

    $ blksum nbd://localhost
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  nbd://localhost

The `blksum` command does not support yet secure `NBD` connections, so its
use for accessing images on remote hosts is limited.

## Computing a checksum from a pipe

`blksum` can compute a checksum for data read from a pipe:

    $ cat disk.raw | blksum
    d9ec2aff0fd7be0385e6331ffe113ca3360e9665afe2f113f37d1dd89d16324e  -

Using a pipe we can only use `raw` format, and the operation is less
efficient for sparse images.

## Configurable digest algorithms

The `blksum` command supports any message digest algorithm provided by
`openssl`. The default digest is `sha256` which is a safe choice but not
the fastest option. For best interoperability with other users of
`blksum` keep the default.

To use other digests use the `--digest` option:

    $ blksum --digest blake2b512 disk.qcow2
    43712ac8ea97686c1649b7d0d36218bcfab4bd4c22f727aa3bcdba124c24c435b9c12b8071d1839ca2ddca8c96697e4a54989ba817c5d3c966fbeb6f90a98c08  disk.qcow2

To find the available digest names run:

    $ blksum --list-digests
    blake2b512
    blake2s256
    md5
    md5-sha1
    ripemd160
    sha1
    sha224
    sha256
    sha3-224
    sha3-256
    sha3-384
    sha3-512
    sha384
    sha512
    sha512-224
    sha512-256
    shake128
    shake256
    sm3

If you use a non-default digest to compute a checksum other users will
have to use the same digest to verify an image using your checksum.

## Manual page

See [blksum(1)](../man/blksum.1.adoc) for the manual.

## blksum performance

The `blksum` command uses *qemu-nbd* to get guest data from various
image formats and detect image sparseness, and use the `blkhash` library
to compute image checksum.

For typical 25% full sparse image, `blksum` is 15 times faster compared
with standard tools, using 3.7 times less cpu time:

```
$ hyperfine -w1 -p "sleep 2" "blksum 25p.raw" "sha256sum 25p.raw"
Benchmark 1: blksum 25p.raw
  Time (mean ± σ):     877.0 ms ±  33.3 ms    [User: 3380.8 ms, System: 606.0 ms]
  Range (min … max):   803.5 ms … 914.8 ms    10 runs

Benchmark 2: sha256sum 25p.raw
  Time (mean ± σ):     13.275 s ±  0.438 s    [User: 12.570 s, System: 0.672 s]
  Range (min … max):   12.828 s … 14.027 s    10 runs

Summary
  'blksum 25p.raw' ran
   15.14 ± 0.76 times faster than 'sha256sum 25p.raw'
```

For images with more data the speedup is smaller, and more cpu time is
used:

```
$ hyperfine -w1 -p "sleep 2" "blksum 50p.raw" "sha256sum 50p.raw"
Benchmark 1: blksum 50p.raw
  Time (mean ± σ):      1.951 s ±  0.151 s    [User: 7.522 s, System: 1.263 s]
  Range (min … max):    1.733 s …  2.200 s    10 runs

Benchmark 2: sha256sum 50p.raw
  Time (mean ± σ):     13.716 s ±  0.428 s    [User: 13.026 s, System: 0.660 s]
  Range (min … max):   12.903 s … 14.200 s    10 runs

Summary
  'blksum 50p.raw' ran
    7.03 ± 0.59 times faster than 'sha256sum 50p.raw'
```

But even when the image is 75% full `blkusum` is 4.5 time faster:

```
$ hyperfine -w1 -p "sleep 2" "blksum 75p.raw" "sha256sum 75p.raw"
Benchmark 1: blksum 75p.raw
  Time (mean ± σ):      3.019 s ±  0.233 s    [User: 12.063 s, System: 1.915 s]
  Range (min … max):    2.725 s …  3.518 s    10 runs

Benchmark 2: sha256sum 75p.raw
  Time (mean ± σ):     13.622 s ±  0.541 s    [User: 12.850 s, System: 0.737 s]
  Range (min … max):   13.028 s … 14.579 s    10 runs

Summary
  'blksum 75p.raw' ran
    4.51 ± 0.39 times faster than 'sha256sum 75p.raw'
```

When reading from a pipe we can use only raw image data and cannot
detect image sparseness, but zero detection and multiple threads are
effective:

```
$ hyperfine -w1 -p "sleep 2" "blksum <50p.raw" "sha256sum <50p.raw"
Benchmark 1: blksum <50p.raw
  Time (mean ± σ):      1.895 s ±  0.048 s    [User: 6.426 s, System: 0.622 s]
  Range (min … max):    1.825 s …  1.986 s    10 runs

Benchmark 2: sha256sum <50p.raw
  Time (mean ± σ):     13.039 s ±  0.536 s    [User: 12.369 s, System: 0.642 s]
  Range (min … max):   12.317 s … 14.103 s    10 runs

Summary
  'blksum <50p.raw' ran
    6.88 ± 0.33 times faster than 'sha256sum <50p.raw'
```

The best case is a completely empty image. `blksum` detects that the
entire image is unallocated without reading any data from storage and
optimize zero hashing:

```
$ hyperfine -w1 -p "sleep 2" "blksum empty-6g.raw" "sha256sum empty-6g.raw"
Benchmark 1: blksum empty-6g.raw
  Time (mean ± σ):      37.1 ms ±   6.8 ms    [User: 11.3 ms, System: 5.1 ms]
  Range (min … max):    27.1 ms …  44.1 ms    10 runs

Benchmark 2: sha256sum empty-6g.raw
  Time (mean ± σ):     13.522 s ±  0.557 s    [User: 12.795 s, System: 0.692 s]
  Range (min … max):   12.545 s … 14.190 s    10 runs

Summary
  'blksum empty-6g.raw' ran
  364.58 ± 68.72 times faster than 'sha256sum empty-6g.raw'
```

A less optimal case is a fully allocated image full of zeros. `blksum`
must read the entire image, but it detects that all blocks are zeros
and optimize zero hashing:

```
$ hyperfine -w1 -p "sleep 2" "blksum zero-6g.raw" "sha256sum zero-6g.raw"
Benchmark 1: blksum zero-6g.raw
  Time (mean ± σ):      2.041 s ±  0.011 s    [User: 0.446 s, System: 1.786 s]
  Range (min … max):    2.026 s …  2.065 s    10 runs

Benchmark 2: sha256sum zero-6g.raw
  Time (mean ± σ):     13.683 s ±  0.537 s    [User: 12.854 s, System: 0.790 s]
  Range (min … max):   13.024 s … 14.661 s    10 runs

Summary
  'blksum zero-6g.raw' ran
    6.70 ± 0.27 times faster than 'sha256sum zero-6g.raw'
```

The worst case is a completely full image, when nothing can be
optimized, but using multi-threading helps:

```
$ hyperfine -w1 -p "sleep 2" "blksum full-6g.raw" "sha256sum full-6g.raw"
Benchmark 1: blksum full-6g.raw
  Time (mean ± σ):      4.227 s ±  0.252 s    [User: 17.021 s, System: 2.683 s]
  Range (min … max):    3.777 s …  4.493 s    10 runs

Benchmark 2: sha256sum full-6g.raw
  Time (mean ± σ):     13.386 s ±  0.453 s    [User: 12.689 s, System: 0.655 s]
  Range (min … max):   13.006 s … 14.104 s    10 runs

Summary
  'blksum full-6g.raw' ran
    3.17 ± 0.22 times faster than 'sha256sum full-6g.raw'
```

### blksum --queue-size option

The `--queue-size` option limit the total size of in-flight `NBD`
requests. Testing shows that it can change the throughput by 10%,
depending on the machine. The default value (2097152) works well on on
tested machines.

Example run on *Lenovo ThinkPad P1 Gen 3* (i7-10850H CPU @ 2.70GHz) with
*Fedora 36*:

```
$ hyperfine -p "sleep 3" -L q 1048576,2097152,4194304,8388608 "blksum --queue-size {q} 50p.raw"
Benchmark 1: blksum --queue-size 1048576 50p.raw
  Time (mean ± σ):      1.873 s ±  0.066 s    [User: 7.274 s, System: 1.213 s]
  Range (min … max):    1.811 s …  2.014 s    10 runs

Benchmark 2: blksum --queue-size 2097152 50p.raw
  Time (mean ± σ):      1.926 s ±  0.046 s    [User: 7.541 s, System: 1.228 s]
  Range (min … max):    1.826 s …  2.006 s    10 runs

Benchmark 3: blksum --queue-size 4194304 50p.raw
  Time (mean ± σ):      1.988 s ±  0.070 s    [User: 7.857 s, System: 1.307 s]
  Range (min … max):    1.879 s …  2.103 s    10 runs

Benchmark 4: blksum --queue-size 8388608 50p.raw
  Time (mean ± σ):      2.043 s ±  0.092 s    [User: 8.035 s, System: 1.427 s]
  Range (min … max):    1.954 s …  2.222 s    10 runs

Summary
  'blksum --queue-size 1048576 50p.raw' ran
    1.03 ± 0.04 times faster than 'blksum --queue-size 2097152 50p.raw'
    1.06 ± 0.05 times faster than 'blksum --queue-size 4194304 50p.raw'
    1.09 ± 0.06 times faster than 'blksum --queue-size 8388608 50p.raw'
```

Example run on *Dell PowerEdge R640* (Xeon(R) Gold 5218R CPU @ 2.10GHz)
with *RHEL 8.5*:

```
# hyperfine -w1 -L q 1048576,2097152,4194304,8388608 "blksum --queue-size {q} 50p.raw"
Benchmark 1: blksum --queue-size 1048576 50p.raw
  Time (mean ± σ):      1.877 s ±  0.026 s    [User: 7.245 s, System: 1.271 s]
  Range (min … max):    1.852 s …  1.926 s    10 runs

Benchmark 2: blksum --queue-size 2097152 50p.raw
  Time (mean ± σ):      1.718 s ±  0.023 s    [User: 7.302 s, System: 1.221 s]
  Range (min … max):    1.691 s …  1.772 s    10 runs

Benchmark 3: blksum --queue-size 4194304 50p.raw
  Time (mean ± σ):      1.697 s ±  0.014 s    [User: 7.233 s, System: 1.261 s]
  Range (min … max):    1.680 s …  1.726 s    10 runs

Benchmark 4: blksum --queue-size 8388608 50p.raw
  Time (mean ± σ):      1.700 s ±  0.017 s    [User: 7.253 s, System: 1.261 s]
  Range (min … max):    1.680 s …  1.740 s    10 runs

Summary
  'blksum --queue-size 4194304 50p.raw' ran
    1.00 ± 0.01 times faster than 'blksum --queue-size 8388608 50p.raw'
    1.01 ± 0.02 times faster than 'blksum --queue-size 2097152 50p.raw'
    1.11 ± 0.02 times faster than 'blksum --queue-size 1048576 50p.raw'
```
