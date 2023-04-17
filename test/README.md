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

## The bench-data.py script

This script compare the throughput of `blkhash` for hashing non-zero data
with different number of threads.

Example run of the benchmark script:

    $ test/bench-data.py

    blkhash-bench --digest-name sha256 --input-type data

     1 threads, 64 streams: 4.65 GiB in 10.004 s (476.43 MiB/s)
     2 threads, 64 streams: 9.01 GiB in 10.003 s (921.89 MiB/s)
     4 threads, 64 streams: 17.48 GiB in 10.003 s (1.75 GiB/s)
     8 threads, 64 streams: 34.60 GiB in 10.003 s (3.46 GiB/s)
    16 threads, 64 streams: 55.18 GiB in 10.000 s (5.52 GiB/s)
    32 threads, 64 streams: 52.16 GiB in 10.001 s (5.22 GiB/s)
    64 threads, 64 streams: 59.07 GiB in 10.001 s (5.91 GiB/s)

## The bench-zero.py script

This script compare the throughput of `blkhash` for hashing data which is
all zeros with different number of threads.

Example run of the benchmark script:

$ test/bench-zero.py

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads, 64 streams: 380.86 GiB in 10.078 s (37.79 GiB/s)
     2 threads, 64 streams: 420.76 GiB in 10.025 s (41.97 GiB/s)
     4 threads, 64 streams: 410.31 GiB in 10.011 s (40.99 GiB/s)
     8 threads, 64 streams: 428.40 GiB in 10.009 s (42.80 GiB/s)
    16 threads, 64 streams: 380.65 GiB in 10.008 s (38.04 GiB/s)
    32 threads, 64 streams: 403.36 GiB in 10.003 s (40.33 GiB/s)
    64 threads, 64 streams: 388.20 GiB in 10.001 s (38.82 GiB/s)

## The bench-hole.py script

This script compare the throughput of `blkhash` for hashing unallocated
data with different number of threads.

Example run of the benchmark script:

    $ test/bench-hole.py

    blkhash-bench --digest-name sha256 --input-type hole

     1 threads, 64 streams: 7.50 TiB in 10.040 s (764.98 GiB/s)
     2 threads, 64 streams: 14.50 TiB in 10.024 s (1.45 TiB/s)
     4 threads, 64 streams: 28.69 TiB in 10.042 s (2.86 TiB/s)
     8 threads, 64 streams: 54.38 TiB in 10.028 s (5.42 TiB/s)
    16 threads, 64 streams: 91.06 TiB in 10.028 s (9.08 TiB/s)
    32 threads, 64 streams: 87.81 TiB in 10.033 s (8.75 TiB/s)
    64 threads, 64 streams: 146.25 TiB in 10.030 s (14.58 TiB/s)

## The bench-openssl.py script

This script compare the throughput of `sha256` for hashing data with
different number of threads. This shows the highest possible throughput
on the machine, for evaluating `blkhash` efficiency.

Example run of the benchmark script:

$ test/bench-openssl.py

    openssl-bench --digest-name sha256

     1 threads: 4.83 GiB in 10.002 s (494.71 MiB/s)
     2 threads: 9.66 GiB in 10.001 s (988.57 MiB/s)
     4 threads: 18.35 GiB in 10.002 s (1.83 GiB/s)
     8 threads: 36.28 GiB in 10.002 s (3.63 GiB/s)
    16 threads: 68.60 GiB in 10.003 s (6.86 GiB/s)
    32 threads: 118.94 GiB in 10.004 s (11.89 GiB/s)
    64 threads: 141.33 GiB in 10.008 s (14.12 GiB/s)

## The bench-null.py script

This script compare the throughput of `blkhash` for hashing data, zero,
and holes using "null" digest and different number of threads. This is
useful for optimizing `blkhash` internals.

Example run of the benchmark script:

    $ test/bench-null.py

    blkhash-bench --digest-name null --input-type data

     1 threads, 64 streams: 176.33 GiB in 10.000 s (17.63 GiB/s)
     2 threads, 64 streams: 170.41 GiB in 10.000 s (17.04 GiB/s)
     4 threads, 64 streams: 165.37 GiB in 10.000 s (16.54 GiB/s)
     8 threads, 64 streams: 163.38 GiB in 10.000 s (16.34 GiB/s)
    16 threads, 64 streams: 152.12 GiB in 10.000 s (15.21 GiB/s)
    32 threads, 64 streams: 145.57 GiB in 10.000 s (14.56 GiB/s)
    64 threads, 64 streams: 147.42 GiB in 10.001 s (14.74 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads, 64 streams: 411.12 GiB in 10.001 s (41.11 GiB/s)
     2 threads, 64 streams: 426.54 GiB in 10.001 s (42.65 GiB/s)
     4 threads, 64 streams: 406.94 GiB in 10.000 s (40.69 GiB/s)
     8 threads, 64 streams: 413.80 GiB in 10.000 s (41.38 GiB/s)
    16 threads, 64 streams: 424.58 GiB in 10.000 s (42.46 GiB/s)
    32 threads, 64 streams: 414.87 GiB in 10.001 s (41.48 GiB/s)
    64 threads, 64 streams: 403.69 GiB in 10.001 s (40.37 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads, 64 streams: 193.12 TiB in 10.001 s (19.31 TiB/s)
     2 threads, 64 streams: 379.19 TiB in 10.001 s (37.91 TiB/s)
     4 threads, 64 streams: 735.19 TiB in 10.002 s (73.51 TiB/s)
     8 threads, 64 streams: 1.41 TiB in 10.001 s (144.35 TiB/s)
    16 threads, 64 streams: 2.51 TiB in 10.001 s (257.15 TiB/s)
    32 threads, 64 streams: 3.39 TiB in 10.001 s (347.38 TiB/s)
    64 threads, 64 streams: 4.59 TiB in 10.001 s (470.14 TiB/s)

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
