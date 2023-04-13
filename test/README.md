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
                      [-a|--aio] [-q N|--queue-depth N]
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
      "aio": false,
      "queue-depth": 16,
      "block-size": 65536,
      "read-size": 262144,
      "hole-size": 17179869184,
      "threads": 16,
      "streams": 32,
      "total-size": 6123159552,
      "elapsed": 1.003,
      "throughput": 6102697208,
      "checksum": "363f6eb4139b59a955b2cb15229c65c693db0185bff7fe1834f5fd57cd3fb753"
    }

Measure hashing throughput for unallocated data using 16 threads:

    $ build/test/blkhash-bench -i hole -t 16
    {
      "input-type": "hole",
      "digest-name": "sha256",
      "timeout-seconds": 1,
      "input-size": 0,
      "aio": false,
      "queue-depth": 16,
      "block-size": 65536,
      "read-size": 262144,
      "hole-size": 17179869184,
      "threads": 16,
      "streams": 32,
      "total-size": 7627861917696,
      "elapsed": 1.029,
      "throughput": 7409583011752,
      "checksum": "1bc6b0c013f50d34e0e1589137a55bfc8d06cc4bc710264a0e828a7f7425a4f9"
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

The JSON output can be used by another program to create graphs.

### Profiling blkhash

You can use `perf` to profile specific scenarios. In this example we
profile `blkhash` with `sha256` digest, hashing 4 TiB hole.

    $ perf record --call-graph lbr build/test/blkhash-bench -i hole -s 4t
    ...

    $ perf report --stdio

    # Children      Self  Command        Shared Object        Symbol
    # ........  ........  .............  ...................  ....................................
    #
        99.95%     0.00%  blkhash-bench  libc-2.28.so         [.] __clone
            |
             --99.95%--__clone
                       |
                        --99.95%--start_thread
                                  |
                                   --99.95%--worker_thread
                                             |
                                             |--83.41%--stream_update
                                             |          |
                                             |          |--74.85%--SHA256_Update
                                             |          |          |
                                             |          |          |--74.21%--0x7ff3f884ea80
                                             |          |          |
                                             |          |           --0.64%--0x7ff3f86ee120
                                             |          |
                                             |           --7.70%--EVP_DigestUpdate@plt
                                             |
                                              --16.52%--stream_update@plt
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

     1 threads, 64 streams: 4.73 GiB in 10.003 s (484.57 MiB/s)
     2 threads, 64 streams: 9.02 GiB in 10.003 s (923.63 MiB/s)
     4 threads, 64 streams: 17.46 GiB in 10.002 s (1.75 GiB/s)
     8 threads, 64 streams: 34.80 GiB in 10.003 s (3.48 GiB/s)
    16 threads, 64 streams: 58.12 GiB in 10.000 s (5.81 GiB/s)
    32 threads, 64 streams: 55.82 GiB in 10.001 s (5.58 GiB/s)
    64 threads, 64 streams: 63.95 GiB in 10.001 s (6.39 GiB/s)

    blkhash-bench --digest-name sha256 --input-type data --aio

     1 threads, 64 streams: 4.71 GiB in 10.003 s (482.09 MiB/s)
     2 threads, 64 streams: 9.05 GiB in 10.002 s (926.17 MiB/s)
     4 threads, 64 streams: 17.62 GiB in 10.002 s (1.76 GiB/s)
     8 threads, 64 streams: 34.81 GiB in 10.001 s (3.48 GiB/s)
    16 threads, 64 streams: 61.30 GiB in 10.001 s (6.13 GiB/s)
    32 threads, 64 streams: 72.82 GiB in 10.001 s (7.28 GiB/s)
    64 threads, 64 streams: 112.35 GiB in 10.001 s (11.23 GiB/s)

## The bench-zero.py script

This script compare the throughput of `blkhash` for hashing data which is
all zeros with different number of threads.

Example run of the benchmark script:

    $ test/bench-zero.py

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads, 64 streams: 498.59 GiB in 10.065 s (49.54 GiB/s)
     2 threads, 64 streams: 511.08 GiB in 10.054 s (50.83 GiB/s)
     4 threads, 64 streams: 521.07 GiB in 10.004 s (52.08 GiB/s)
     8 threads, 64 streams: 520.89 GiB in 10.002 s (52.08 GiB/s)
    16 threads, 64 streams: 520.38 GiB in 10.002 s (52.03 GiB/s)
    32 threads, 64 streams: 519.28 GiB in 10.001 s (51.92 GiB/s)
    64 threads, 64 streams: 520.01 GiB in 10.001 s (51.99 GiB/s)

    blkhash-bench --digest-name sha256 --input-type zero --aio

     1 threads, 64 streams: 457.53 GiB in 10.012 s (45.70 GiB/s)
     2 threads, 64 streams: 471.64 GiB in 10.020 s (47.07 GiB/s)
     4 threads, 64 streams: 481.90 GiB in 10.015 s (48.12 GiB/s)
     8 threads, 64 streams: 481.54 GiB in 10.009 s (48.11 GiB/s)
    16 threads, 64 streams: 478.23 GiB in 10.005 s (47.80 GiB/s)
    32 threads, 64 streams: 480.82 GiB in 10.005 s (48.06 GiB/s)
    64 threads, 64 streams: 479.86 GiB in 10.003 s (47.97 GiB/s)

## The bench-hole.py script

This script compare the throughput of `blkhash` for hashing unallocated
data with different number of threads.

Example run of the benchmark script:

    $ test/bench-hole.py

    blkhash-bench --digest-name sha256 --input-type hole

     1 threads, 64 streams: 7.69 TiB in 10.093 s (779.91 GiB/s)
     2 threads, 64 streams: 11.56 TiB in 10.062 s (1.15 TiB/s)
     4 threads, 64 streams: 27.69 TiB in 10.045 s (2.76 TiB/s)
     8 threads, 64 streams: 37.12 TiB in 10.043 s (3.70 TiB/s)
    16 threads, 64 streams: 60.94 TiB in 10.037 s (6.07 TiB/s)
    32 threads, 64 streams: 82.12 TiB in 10.032 s (8.19 TiB/s)
    64 threads, 64 streams: 110.75 TiB in 10.048 s (11.02 TiB/s)

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

     1 threads, 64 streams: 98.82 GiB in 10.000 s (9.88 GiB/s)
     2 threads, 64 streams: 200.82 GiB in 10.000 s (20.08 GiB/s)
     4 threads, 64 streams: 205.50 GiB in 10.000 s (20.55 GiB/s)
     8 threads, 64 streams: 199.71 GiB in 10.000 s (19.97 GiB/s)
    16 threads, 64 streams: 189.11 GiB in 10.000 s (18.91 GiB/s)
    32 threads, 64 streams: 169.32 GiB in 10.000 s (16.93 GiB/s)
    64 threads, 64 streams: 169.98 GiB in 10.001 s (17.00 GiB/s)

    blkhash-bench --digest-name null --input-type data --aio

     1 threads, 64 streams: 1.08 TiB in 10.000 s (110.87 GiB/s)
     2 threads, 64 streams: 824.90 GiB in 10.000 s (82.49 GiB/s)
     4 threads, 64 streams: 415.05 GiB in 10.000 s (41.50 GiB/s)
     8 threads, 64 streams: 452.16 GiB in 10.000 s (45.22 GiB/s)
    16 threads, 64 streams: 428.71 GiB in 10.000 s (42.87 GiB/s)
    32 threads, 64 streams: 395.40 GiB in 10.000 s (39.54 GiB/s)
    64 threads, 64 streams: 399.47 GiB in 10.001 s (39.95 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads, 64 streams: 520.05 GiB in 10.001 s (52.00 GiB/s)
     2 threads, 64 streams: 521.06 GiB in 10.000 s (52.10 GiB/s)
     4 threads, 64 streams: 520.95 GiB in 10.000 s (52.09 GiB/s)
     8 threads, 64 streams: 519.92 GiB in 10.000 s (51.99 GiB/s)
    16 threads, 64 streams: 521.60 GiB in 10.000 s (52.16 GiB/s)
    32 threads, 64 streams: 520.75 GiB in 10.001 s (52.07 GiB/s)
    64 threads, 64 streams: 520.15 GiB in 10.001 s (52.01 GiB/s)

    blkhash-bench --digest-name null --input-type zero --aio

     1 threads, 64 streams: 479.89 GiB in 10.002 s (47.98 GiB/s)
     2 threads, 64 streams: 480.31 GiB in 10.001 s (48.02 GiB/s)
     4 threads, 64 streams: 479.45 GiB in 10.001 s (47.94 GiB/s)
     8 threads, 64 streams: 481.37 GiB in 10.001 s (48.13 GiB/s)
    16 threads, 64 streams: 480.86 GiB in 10.001 s (48.08 GiB/s)
    32 threads, 64 streams: 479.76 GiB in 10.001 s (47.97 GiB/s)
    64 threads, 64 streams: 479.46 GiB in 10.001 s (47.94 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads, 64 streams: 206.75 TiB in 10.002 s (20.67 TiB/s)
     2 threads, 64 streams: 395.19 TiB in 10.002 s (39.51 TiB/s)
     4 threads, 64 streams: 609.62 TiB in 10.001 s (60.96 TiB/s)
     8 threads, 64 streams: 507.38 TiB in 10.001 s (50.73 TiB/s)
    16 threads, 64 streams: 734.44 TiB in 10.001 s (73.43 TiB/s)
    32 threads, 64 streams: 751.06 TiB in 10.002 s (75.09 TiB/s)
    64 threads, 64 streams: 677.06 TiB in 10.002 s (67.69 TiB/s)

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

    $ build/test/zero-bench | grep -v PASS
    aligned data best: 40.00 TiB in 1.349 seconds (29.65 TiB/s)
    aligned data worst: 60.00 GiB in 1.130 seconds (53.10 GiB/s)
    aligned zero: 60.00 GiB in 1.132 seconds (53.02 GiB/s)
    unaligned data best: 40.00 TiB in 1.347 seconds (29.69 TiB/s)
    unaligned data worst: 60.00 GiB in 1.133 seconds (52.96 GiB/s)
    unaligned zero: 60.00 GiB in 1.133 seconds (52.98 GiB/s)

### MacBook Air M1

    % build/test/zero-bench
    aligned data best: 40.00 TiB in 0.878 seconds (45.54 TiB/s)
    aligned data worst: 60.00 GiB in 1.659 seconds (36.17 GiB/s)
    aligned zero: 60.00 GiB in 1.637 seconds (36.65 GiB/s)
    unaligned data best: 40.00 TiB in 0.839 seconds (47.69 TiB/s)
    unaligned data worst: 60.00 GiB in 1.666 seconds (36.02 GiB/s)
    unaligned zero: 60.00 GiB in 1.651 seconds (36.34 GiB/s)
