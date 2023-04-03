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

        openssl-bench [-d DIGEST|--digest-name=DIGEST]
                      [-T N|--timeout-seconds=N]
                      [-s N|--input-size N]
                      [-r N|--read-size N] [-h|--help]

Example run:

    $ build/test/openssl-bench
    {
      "input-type": "data",
      "digest-name": "sha256",
      "timeout-seconds": 1.000,
      "input-size": 0,
      "read-size": 1048576,
      "threads": 1,
      "total-size": 617611264,
      "elapsed": 1.001,
      "throughput": 616829740,
      "checksum": "6ac87117718f7ffdf46609908a26bdf2336efe6844aa450b3df1c61214158710"
    }

## The blkhash-bench.py script

This script runs many scenarios using the `blkhash-bench` program.
The script should be used for evaluating performance related changes,
comparing results before and after a change.

Example run of the benchmark script:

    $ test/blkhash-bench.py

    blkhash-bench --digest-name sha256 --input-type data

     1 threads, 64 streams: 480.00 MiB in 1.002 s (478.87 MiB/s)
     2 threads, 64 streams: 926.00 MiB in 1.003 s (923.03 MiB/s)
     4 threads, 64 streams: 1.76 GiB in 1.003 s (1.76 GiB/s)
     8 threads, 64 streams: 3.36 GiB in 1.003 s (3.35 GiB/s)
    16 threads, 64 streams: 5.53 GiB in 1.003 s (5.51 GiB/s)
    32 threads, 64 streams: 5.97 GiB in 1.006 s (5.94 GiB/s)
    64 threads, 64 streams: 6.36 GiB in 1.001 s (6.35 GiB/s)

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads, 64 streams: 41.75 GiB in 1.056 s (39.54 GiB/s)
     2 threads, 64 streams: 42.28 GiB in 1.030 s (41.06 GiB/s)
     4 threads, 64 streams: 44.64 GiB in 1.016 s (43.93 GiB/s)
     8 threads, 64 streams: 41.03 GiB in 1.009 s (40.67 GiB/s)
    16 threads, 64 streams: 42.72 GiB in 1.006 s (42.47 GiB/s)
    32 threads, 64 streams: 40.54 GiB in 1.005 s (40.32 GiB/s)
    64 threads, 64 streams: 43.17 GiB in 1.005 s (42.96 GiB/s)

    blkhash-bench --digest-name sha256 --input-type hole

     1 threads, 64 streams: 832.00 GiB in 1.071 s (776.69 GiB/s)
     2 threads, 64 streams: 1.50 TiB in 1.026 s (1.46 TiB/s)
     4 threads, 64 streams: 2.94 TiB in 1.033 s (2.84 TiB/s)
     8 threads, 64 streams: 5.56 TiB in 1.031 s (5.40 TiB/s)
    16 threads, 64 streams: 9.12 TiB in 1.034 s (8.82 TiB/s)
    32 threads, 64 streams: 8.94 TiB in 1.034 s (8.64 TiB/s)
    64 threads, 64 streams: 14.12 TiB in 1.032 s (13.68 TiB/s)

    blkhash-bench --digest-name null --input-type data

     1 threads, 64 streams: 16.73 GiB in 1.000 s (16.73 GiB/s)
     2 threads, 64 streams: 17.02 GiB in 1.000 s (17.02 GiB/s)
     4 threads, 64 streams: 16.19 GiB in 1.000 s (16.19 GiB/s)
     8 threads, 64 streams: 16.48 GiB in 1.000 s (16.48 GiB/s)
    16 threads, 64 streams: 14.41 GiB in 1.000 s (14.41 GiB/s)
    32 threads, 64 streams: 14.33 GiB in 1.000 s (14.32 GiB/s)
    64 threads, 64 streams: 14.29 GiB in 1.001 s (14.29 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads, 64 streams: 39.83 GiB in 1.002 s (39.74 GiB/s)
     2 threads, 64 streams: 42.75 GiB in 1.001 s (42.70 GiB/s)
     4 threads, 64 streams: 44.67 GiB in 1.001 s (44.64 GiB/s)
     8 threads, 64 streams: 42.17 GiB in 1.001 s (42.14 GiB/s)
    16 threads, 64 streams: 43.39 GiB in 1.001 s (43.37 GiB/s)
    32 threads, 64 streams: 39.12 GiB in 1.001 s (39.08 GiB/s)
    64 threads, 64 streams: 41.74 GiB in 1.001 s (41.68 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads, 64 streams: 20.69 TiB in 1.001 s (20.66 TiB/s)
     2 threads, 64 streams: 39.44 TiB in 1.002 s (39.35 TiB/s)
     4 threads, 64 streams: 76.75 TiB in 1.001 s (76.66 TiB/s)
     8 threads, 64 streams: 146.19 TiB in 1.001 s (146.00 TiB/s)
    16 threads, 64 streams: 233.19 TiB in 1.001 s (232.89 TiB/s)
    32 threads, 64 streams: 298.94 TiB in 1.001 s (298.50 TiB/s)
    64 threads, 64 streams: 466.12 TiB in 1.001 s (465.72 TiB/s)

    openssl-bench --digest-name sha256

     1 threads: 476.00 MiB in 1.000 s (475.97 MiB/s)

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
