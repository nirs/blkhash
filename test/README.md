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

        blkhash-bench [-i TYPE|--input-type TYPE] [-s N|--input-size N]
                      [-d DIGEST|--digest-name=DIGEST] [-b N|--block-size N]
                      [-r N|--read-size N] [-z N|--hole-size N]
                      [-t N|--threads N] [-h|--help]

    input types:
        data: non-zero data
        zero: all zeros data
        hole: unallocated area

### Example run

This example benchmark hashing of 1 GiB of non-zero data with `sha256`
digest:

    $ build/test/blkhash-bench -i data -s 1g -d sha256
    {
      "input-type": "data",
      "input-size": 1073741824,
      "digest-name": "sha256",
      "block-size": 65536,
      "read-size": 1048576,
      "hole-size": 17179869184,
      "threads": 4,
      "elapsed": 0.488,
      "throughput": 2199118961
    }

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

     1 threads: 614.40 MiB in 1.062 s (578.72 MiB/s)
     2 threads: 1.00 GiB in 0.932 s (1.07 GiB/s)
     4 threads: 2.00 GiB in 0.987 s (2.03 GiB/s)
     8 threads: 2.20 GiB in 0.985 s (2.23 GiB/s)
    16 threads: 3.00 GiB in 0.955 s (3.14 GiB/s)
    32 threads: 2.90 GiB in 0.975 s (2.97 GiB/s)

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads: 50.00 GiB in 1.080 s (46.32 GiB/s)
     2 threads: 50.00 GiB in 1.054 s (47.44 GiB/s)
     4 threads: 50.00 GiB in 1.083 s (46.16 GiB/s)
     8 threads: 50.00 GiB in 0.972 s (51.43 GiB/s)
    16 threads: 50.00 GiB in 1.060 s (47.18 GiB/s)
    32 threads: 50.00 GiB in 0.980 s (51.04 GiB/s)

    blkhash-bench --digest-name sha256 --input-type hole

     1 threads: 960.00 GiB in 1.046 s (917.55 GiB/s)
     2 threads: 1.80 TiB in 0.997 s (1.81 TiB/s)
     4 threads: 3.50 TiB in 1.048 s (3.34 TiB/s)
     8 threads: 4.00 TiB in 1.102 s (3.63 TiB/s)
    16 threads: 4.70 TiB in 1.036 s (4.54 TiB/s)
    32 threads: 4.60 TiB in 1.032 s (4.46 TiB/s)

    blkhash-bench --digest-name null --input-type data

     1 threads: 21.00 GiB in 1.477 s (14.22 GiB/s)
     2 threads: 21.00 GiB in 0.834 s (25.17 GiB/s)
     4 threads: 21.00 GiB in 0.891 s (23.57 GiB/s)
     8 threads: 21.00 GiB in 0.959 s (21.90 GiB/s)
    16 threads: 21.00 GiB in 0.870 s (24.13 GiB/s)
    32 threads: 21.00 GiB in 0.984 s (21.35 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads: 50.00 GiB in 1.006 s (49.72 GiB/s)
     2 threads: 50.00 GiB in 1.067 s (46.86 GiB/s)
     4 threads: 50.00 GiB in 1.077 s (46.41 GiB/s)
     8 threads: 50.00 GiB in 1.073 s (46.62 GiB/s)
    16 threads: 50.00 GiB in 0.974 s (51.33 GiB/s)
    32 threads: 50.00 GiB in 1.071 s (46.70 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads: 31.00 TiB in 0.961 s (32.27 TiB/s)
     2 threads: 61.00 TiB in 0.961 s (63.50 TiB/s)
     4 threads: 118.00 TiB in 0.963 s (122.53 TiB/s)
     8 threads: 157.00 TiB in 1.035 s (151.65 TiB/s)
    16 threads: 211.00 TiB in 1.014 s (208.16 TiB/s)
    32 threads: 190.00 TiB in 0.932 s (203.76 TiB/s)

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
