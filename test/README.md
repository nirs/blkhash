<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Testing blkhash

This directory contains various tests and benchmarks for testing the
blkhash library and the blksum command.

This is work in progress, describing only some of the tests.

## The blkhash-bench program

This program can be used to benchmark the `blkhash` library with different
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
profile `blkhash` with `sha256` digest, hashing 4 TiB hole.

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
compare `blkhash` to digest functions provided by openssl.

    $ build/test/openssl-bench -h

    Benchmark openssl

        openssl-bench [-d DIGEST|--digest-name=DIGEST]
                      [-T N|--timeout-seconds=N]
                      [-s N|--input-size N] [-r N|--read-size N]
                      [-t N|--threads N] [-h|--help]

Running single threaded benchmark:

    $ build/test/openssl-bench
    {
      "input-type": "data",
      "digest-name": "sha256",
      "timeout-seconds": 1,
      "input-size": 0,
      "read-size": 1048576,
      "threads": 1,
      "total-size": 619708416,
      "elapsed": 1.001,
      "throughput": 618973075,
      "checksum": "51e4a452b62b8f15a457c0cf9e4fff596493f52a2ca48c0c31dab8dcb4140c19"
    }

Using multiple threads we can evaluate the maximum scalability of
`blkhash` on a machine:

    $ build/test/openssl-bench -t 12
    {
      "input-type": "data",
      "digest-name": "sha256",
      "timeout-seconds": 1,
      "input-size": 0,
      "read-size": 1048576,
      "threads": 12,
      "total-size": 3528458240,
      "elapsed": 1.003,
      "throughput": 3516330416,
      "checksum": "3132289183802c3459a0fefa862b46726ff9030eb991060518b0b38bd22d0b53"
    }

When using multiple threads, only the first thread checksum is reported.

## The blkhash-bench.py script

This script runs many scenarios using the `blkhash-bench` program.
The script should be used for evaluating performance related changes,
comparing results before and after a change.

Example run of the benchmark script:

    $ test/blkhash-bench.py

    blkhash-bench --digest-name sha256 --input-type data

     1 threads, 64 streams: 481.00 MiB in 1.004 s (479.19 MiB/s)
     2 threads, 64 streams: 924.00 MiB in 1.003 s (921.08 MiB/s)
     4 threads, 64 streams: 1.76 GiB in 1.003 s (1.76 GiB/s)
     8 threads, 64 streams: 3.33 GiB in 1.003 s (3.32 GiB/s)
    16 threads, 64 streams: 5.65 GiB in 1.003 s (5.63 GiB/s)
    32 threads, 64 streams: 5.98 GiB in 1.006 s (5.94 GiB/s)
    64 threads, 64 streams: 6.37 GiB in 1.001 s (6.37 GiB/s)

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads, 64 streams: 45.05 GiB in 1.058 s (42.59 GiB/s)
     2 threads, 64 streams: 42.07 GiB in 1.028 s (40.91 GiB/s)
     4 threads, 64 streams: 38.66 GiB in 1.014 s (38.13 GiB/s)
     8 threads, 64 streams: 39.75 GiB in 1.008 s (39.42 GiB/s)
    16 threads, 64 streams: 38.96 GiB in 1.006 s (38.75 GiB/s)
    32 threads, 64 streams: 40.36 GiB in 1.005 s (40.15 GiB/s)
    64 threads, 64 streams: 39.93 GiB in 1.005 s (39.74 GiB/s)

    blkhash-bench --digest-name sha256 --input-type hole

     1 threads, 64 streams: 832.00 GiB in 1.078 s (772.10 GiB/s)
     2 threads, 64 streams: 1.50 TiB in 1.028 s (1.46 TiB/s)
     4 threads, 64 streams: 2.94 TiB in 1.038 s (2.83 TiB/s)
     8 threads, 64 streams: 5.44 TiB in 1.029 s (5.29 TiB/s)
    16 threads, 64 streams: 7.81 TiB in 1.037 s (7.53 TiB/s)
    32 threads, 64 streams: 8.94 TiB in 1.035 s (8.63 TiB/s)
    64 threads, 64 streams: 15.00 TiB in 1.035 s (14.50 TiB/s)

    blkhash-bench --digest-name null --input-type data

     1 threads, 64 streams: 16.72 GiB in 1.000 s (16.72 GiB/s)
     2 threads, 64 streams: 16.60 GiB in 1.000 s (16.60 GiB/s)
     4 threads, 64 streams: 16.90 GiB in 1.000 s (16.90 GiB/s)
     8 threads, 64 streams: 16.48 GiB in 1.000 s (16.48 GiB/s)
    16 threads, 64 streams: 14.43 GiB in 1.000 s (14.43 GiB/s)
    32 threads, 64 streams: 14.39 GiB in 1.000 s (14.39 GiB/s)
    64 threads, 64 streams: 14.67 GiB in 1.001 s (14.66 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads, 64 streams: 42.30 GiB in 1.002 s (42.21 GiB/s)
     2 threads, 64 streams: 44.90 GiB in 1.001 s (44.84 GiB/s)
     4 threads, 64 streams: 42.48 GiB in 1.001 s (42.45 GiB/s)
     8 threads, 64 streams: 41.49 GiB in 1.001 s (41.47 GiB/s)
    16 threads, 64 streams: 42.03 GiB in 1.001 s (42.00 GiB/s)
    32 threads, 64 streams: 41.40 GiB in 1.001 s (41.36 GiB/s)
    64 threads, 64 streams: 45.28 GiB in 1.001 s (45.21 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads, 64 streams: 19.75 TiB in 1.001 s (19.72 TiB/s)
     2 threads, 64 streams: 39.56 TiB in 1.002 s (39.47 TiB/s)
     4 threads, 64 streams: 77.06 TiB in 1.001 s (76.99 TiB/s)
     8 threads, 64 streams: 145.81 TiB in 1.001 s (145.65 TiB/s)
    16 threads, 64 streams: 222.62 TiB in 1.001 s (222.34 TiB/s)
    32 threads, 64 streams: 300.19 TiB in 1.001 s (299.80 TiB/s)
    64 threads, 64 streams: 542.06 TiB in 1.002 s (541.23 TiB/s)

    openssl-bench --digest-name sha256

     1 threads: 478.00 MiB in 1.002 s (477.06 MiB/s)
     2 threads: 944.00 MiB in 1.002 s (942.33 MiB/s)
     4 threads: 1.79 GiB in 1.002 s (1.79 GiB/s)
     8 threads: 3.45 GiB in 1.003 s (3.44 GiB/s)
    16 threads: 6.85 GiB in 1.002 s (6.84 GiB/s)
    32 threads: 11.42 GiB in 1.003 s (11.38 GiB/s)
    64 threads: 14.43 GiB in 1.009 s (14.31 GiB/s)

## The zero-bench program

This program measure zero detection throughput.

We measure 3 cases:

- data best: A non zero byte in the first 16 bytes of the buffer. This
  is very likely for non-zero data, and show that zero detection is
  practically free.

- data worst: A non zero byte at the last 16 bytes of the buffer, so
  `blkhash` must scan the entire buffer, and then compute a hash.

- zero: The entire buffer contains zero bytes, so `blkhash` can
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
