<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Testing blkhash

This directory contains various tests and benchmarks for testing the
blkhash library and the blksum command.

This is work in progress, describing only some of the tests.

## The blkhash-bench program

This program can be used to benchmark the blkhash library with different
kind of input and configuration options.

### Options

    $ build/test/blkhash-bench -h

    Benchmark blkhash

        blkhash-bench [-i TYPE|--input-type TYPE]
                      [-d DIGEST|--digest-name=DIGEST]
                      [-T N|--timeout-seconds N] [-s N|--input-size N]
                      [-t N|--threads N] [-S N|--streams N]
                      [-b N|--block-size N] [-r N|--read-size N]
                      [-z N|--hole-size N] [-h|--help]

    input types:
        data: non-zero data
        zero: all zeros data
        hole: unallocated area

### Example usage

Measure hashing throughput for non-zero data using 16 threads:

    $ build/test/blkhash-bench -i data -t 16
    {
      "input-type": "data",
      "digest-name": "sha256",
      "timeout-seconds": 1,
      "input-size": 0,
      "block-size": 65536,
      "read-size": 1048576,
      "hole-size": 17179869184,
      "threads": 16,
      "streams": 32,
      "total-size": 3463446528,
      "elapsed": 1.004,
      "throughput": 3450792472,
      "checksum": "00cfa392d29e721583243ec4bd4c0f19fe47fe87d96969fa1fed66e94c57ba00"
    }

Measure hashing throughput for unallocated data using 16 threads:

    $ build/test/blkhash-bench -i hole -t 16
    {
      "input-type": "hole",
      "digest-name": "sha256",
      "timeout-seconds": 1,
      "input-size": 0,
      "block-size": 65536,
      "read-size": 1048576,
      "hole-size": 17179869184,
      "threads": 16,
      "streams": 32,
      "total-size": 5600637353984,
      "elapsed": 1.032,
      "throughput": 5428983745905,
      "checksum": "6ff9cb9d47f2a9418f7a7315a3c4720d1fb3de2a657f5fae017d44c6c3344ed1"
    }

Validate the checksum for 1 MiB of data with different number of
threads:

    $ for n in 1 2 4 8 16 32; do build/test/blkhash-bench -s 1m -t $n | jq .checksum; done
    "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"
    "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"
    "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"
    "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"
    "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"
    "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"

The json output can be used by another program to create graphs.

### Profiling blkhash

You can use `perf` to profile specific scenarios. In this example we
profile blkhash with `sha256` digest, hashing 4 TiB hole.

    $ perf record -g --call-graph dwarf build/test/blkhash-bench -i hole -s 4t
    ...

    $ perf report --stdio

    # Children      Self  Command        Shared Object         Symbol
    # ........  ........  .............  ....................  .....................................
    #
        12.29%     0.99%  blkhash-bench  libblkhash.so.0.8.0   [.] worker_thread
                |
                |--11.34%--worker_thread
                |          |
                |           --11.32%--add_zero_blocks_before (inlined)
                |                     |
                |                     |--10.14%--SHA256_Update
                |                     |          |
                |                     |           --0.54%--0x7f7c2b4fc0fd
                |                     |
                |                      --0.83%--EVP_DigestUpdate
                |
                 --0.95%--__clone3 (inlined)
                           start_thread
                           worker_thread
                           |
                            --0.95%--add_zero_blocks_before (inlined)
    ...

## The openssl-bench program

Similar to `blkhash-bench` with the relevant options. It is used to
compare blkhash to digest functions provided by openssl.

    $ build/test/openssl-bench -h

    Benchmark openssl

        openssl-bench [-s N|--input-size N]
                      [-d DIGEST|--digest-name=DIGEST]
                      [-r N|--read-size N] [-h|--help]

Example run:

    $ build/test/openssl-bench -s 550m -d sha256
    {
      "input-type": "data",
      "input-size": 576716800,
      "digest-name": "sha256",
      "read-size": 1048576,
      "threads": 1,
      "elapsed": 0.975,
      "throughput": 591693752
    }

## The blkhash-bench.py script

This script runs many scenarios using the `blkhash-bench` program.
The script should be used for evaluating performance related changes,
comparing results before and after a change.

Example run of the benchmark script:

    $ test/blkhash-bench.py

    blkhash-bench --digest-name sha256 --input-type data

     1 threads, 64 streams: 614.40 MiB in 1.385 s (443.63 MiB/s)
     2 threads, 64 streams: 1.00 GiB in 1.159 s (883.90 MiB/s)
     4 threads, 64 streams: 2.00 GiB in 1.155 s (1.73 GiB/s)
     8 threads, 64 streams: 2.20 GiB in 0.676 s (3.25 GiB/s)
    16 threads, 64 streams: 3.00 GiB in 0.548 s (5.47 GiB/s)
    32 threads, 64 streams: 2.90 GiB in 0.490 s (5.91 GiB/s)
    64 threads, 64 streams: 2.90 GiB in 0.466 s (6.23 GiB/s)

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads, 64 streams: 50.00 GiB in 1.326 s (37.71 GiB/s)
     2 threads, 64 streams: 50.00 GiB in 1.219 s (41.01 GiB/s)
     4 threads, 64 streams: 50.00 GiB in 1.275 s (39.22 GiB/s)
     8 threads, 64 streams: 50.00 GiB in 1.170 s (42.72 GiB/s)
    16 threads, 64 streams: 50.00 GiB in 1.223 s (40.87 GiB/s)
    32 threads, 64 streams: 50.00 GiB in 1.330 s (37.60 GiB/s)
    64 threads, 64 streams: 50.00 GiB in 1.230 s (40.64 GiB/s)

    blkhash-bench --digest-name sha256 --input-type hole

     1 threads, 64 streams: 960.00 GiB in 1.296 s (740.80 GiB/s)
     2 threads, 64 streams: 1.80 TiB in 1.263 s (1.43 TiB/s)
     4 threads, 64 streams: 3.50 TiB in 1.258 s (2.78 TiB/s)
     8 threads, 64 streams: 4.00 TiB in 0.790 s (5.07 TiB/s)
    16 threads, 64 streams: 4.70 TiB in 0.536 s (8.77 TiB/s)
    32 threads, 64 streams: 4.60 TiB in 0.528 s (8.72 TiB/s)
    64 threads, 64 streams: 4.60 TiB in 0.346 s (13.31 TiB/s)

    blkhash-bench --digest-name null --input-type data

     1 threads, 64 streams: 21.00 GiB in 1.302 s (16.12 GiB/s)
     2 threads, 64 streams: 21.00 GiB in 1.377 s (15.25 GiB/s)
     4 threads, 64 streams: 21.00 GiB in 1.266 s (16.59 GiB/s)
     8 threads, 64 streams: 21.00 GiB in 1.726 s (12.16 GiB/s)
    16 threads, 64 streams: 21.00 GiB in 1.353 s (15.53 GiB/s)
    32 threads, 64 streams: 21.00 GiB in 1.434 s (14.64 GiB/s)
    64 threads, 64 streams: 21.00 GiB in 1.475 s (14.24 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads, 64 streams: 50.00 GiB in 1.121 s (44.61 GiB/s)
     2 threads, 64 streams: 50.00 GiB in 1.161 s (43.07 GiB/s)
     4 threads, 64 streams: 50.00 GiB in 1.221 s (40.96 GiB/s)
     8 threads, 64 streams: 50.00 GiB in 1.263 s (39.59 GiB/s)
    16 threads, 64 streams: 50.00 GiB in 1.256 s (39.80 GiB/s)
    32 threads, 64 streams: 50.00 GiB in 1.274 s (39.26 GiB/s)
    64 threads, 64 streams: 50.00 GiB in 1.265 s (39.54 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads, 64 streams: 31.00 TiB in 1.589 s (19.51 TiB/s)
     2 threads, 64 streams: 61.00 TiB in 1.589 s (38.38 TiB/s)
     4 threads, 64 streams: 118.00 TiB in 1.593 s (74.07 TiB/s)
     8 threads, 64 streams: 157.00 TiB in 1.097 s (143.06 TiB/s)
    16 threads, 64 streams: 211.00 TiB in 0.967 s (218.15 TiB/s)
    32 threads, 64 streams: 190.00 TiB in 0.626 s (303.66 TiB/s)
    64 threads, 64 streams: 190.00 TiB in 0.344 s (553.09 TiB/s)

## The zero-bench program

This program measure zero detection throughput.

We measure 3 cases:

- data best: A non zero byte in the first 16 bytes of the buffer. This
  is very likely for non-zero data, and show that zero detection is
  practically free.

- data worst: A non zero byte at the last 16 bytes of the buffer, so
  blkhash must scan the entire buffer, and then compute a hash.

- zero: The entire buffer contains zero bytes, so blkhash can
  skip hash computation, but it has to scan the entire buffer.

For each case we measure both aligned and unaligned buffer.

### Lenovo P1 Gen 3 (Intel(R) Core(TM) i7-10850H CPU @ 2.70GHz)

    $ build/test/zero-bench
    aligned data best: 40.00 TiB in 0.983 seconds (40.68 TiB/s)
    aligned data worst: 60.00 GiB in 0.943 seconds (63.65 GiB/s)
    aligned zero: 60.00 GiB in 0.929 seconds (64.58 GiB/s)
    unaligned data best: 40.00 TiB in 0.988 seconds (40.47 TiB/s)
    unaligned data worst: 60.00 GiB in 0.924 seconds (64.94 GiB/s)
    unaligned zero: 60.00 GiB in 0.939 seconds (63.90 GiB/s)

### Dell PowerEdge R640 (Intel(R) Xeon(R) Gold 5218R CPU @ 2.10GHz)

    $ build/test/zero-bench
    aligned data best: 40.00 TiB in 1.388 seconds (28.81 TiB/s)
    aligned data worst: 60.00 GiB in 1.162 seconds (51.64 GiB/s)
    aligned zero: 60.00 GiB in 1.161 seconds (51.69 GiB/s)
    unaligned data best: 40.00 TiB in 1.386 seconds (28.86 TiB/s)
    unaligned data worst: 60.00 GiB in 1.159 seconds (51.75 GiB/s)
    unaligned zero: 60.00 GiB in 1.163 seconds (51.59 GiB/s)

### MacBook Air M1

    % build/test/zero-bench
    aligned data best: 40.00 TiB in 0.878 seconds (45.54 TiB/s)
    aligned data worst: 60.00 GiB in 1.659 seconds (36.17 GiB/s)
    aligned zero: 60.00 GiB in 1.637 seconds (36.65 GiB/s)
    unaligned data best: 40.00 TiB in 0.839 seconds (47.69 TiB/s)
    unaligned data worst: 60.00 GiB in 1.666 seconds (36.02 GiB/s)
    unaligned zero: 60.00 GiB in 1.651 seconds (36.34 GiB/s)
