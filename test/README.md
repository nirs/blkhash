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

Measure hashing throughput for 100 GiB of non-zero data using 64
threads, the async API, and queue depth 32:

```
$ build/test/blkhash-bench --input-type data --input-size 100g \
    --threads 64 --aio --queue-depth 32
{
  "input-type": "data",
  "digest-name": "sha256",
  "timeout-seconds": 1,
  "input-size": 107374182400,
  "aio": true,
  "queue-depth": 32,
  "block-size": 65536,
  "read-size": 262144,
  "hole-size": 17179869184,
  "threads": 64,
  "streams": 64,
  "total-size": 107374182400,
  "elapsed": 8.524,
  "throughput": 12597223134,
  "checksum": "691f0b9dfbcb944b91ad23bac4ea17db82f336b0aa47cec3b6890ee4a39375b6"
}
```

Measure hashing throughput for 100 TiB of unallocated data using 64
threads and queue depth 32:

```
$ build/test/blkhash-bench --input-type hole --input-size 100t \
    --threads 64 --queue-depth 32
{
  "input-type": "hole",
  "digest-name": "sha256",
  "timeout-seconds": 1,
  "input-size": 109951162777600,
  "aio": false,
  "queue-depth": 32,
  "block-size": 65536,
  "read-size": 262144,
  "hole-size": 17179869184,
  "threads": 64,
  "streams": 64,
  "total-size": 109951162777600,
  "elapsed": 9.698,
  "throughput": 11337064827074,
  "checksum": "659cfc90e33af01329e527444482f6b2afd01c827dc06724014b03a1e6583d39"
}
```

Validate the checksum for 1 MiB of data with different number of
threads:

```
$ for n in 1 2 4 8 16 32 64; do build/test/blkhash-bench -s 1m -t $n | jq .checksum; done
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
"2a6d5ed17da97865da0fd3ca9a792f3bfaf325940c44fd6a2f0a224a051eb6f0"
```

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

## The digest-bench program

Similar to `blkhash-bench` with the relevant options. It is used to
compare `blkhash` to digest functions supported by blkhash:

    $ build/test/digest-bench -h

    Benchmark digest

        digest-bench [-d DIGEST|--digest-name=DIGEST]
                     [-T N|--timeout-seconds=N]
                     [-s N|--input-size N] [-r N|--read-size N]
                     [-t N|--threads N] [-h|--help]

Running single threaded benchmark:

    $ build/test/digest-bench
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

    $ build/test/digest-bench -t 12
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

     1 threads, 64 streams: 4.75 GiB in 10.002 s (486.13 MiB/s)
     2 threads, 64 streams: 9.04 GiB in 10.003 s (925.69 MiB/s)
     4 threads, 64 streams: 17.57 GiB in 10.003 s (1.76 GiB/s)
     8 threads, 64 streams: 34.53 GiB in 10.003 s (3.45 GiB/s)
    16 threads, 64 streams: 55.73 GiB in 10.000 s (5.57 GiB/s)
    32 threads, 64 streams: 46.15 GiB in 10.001 s (4.61 GiB/s)
    64 threads, 64 streams: 59.83 GiB in 10.001 s (5.98 GiB/s)

    blkhash-bench --digest-name sha256 --input-type data --aio

     1 threads, 64 streams: 4.74 GiB in 10.003 s (485.36 MiB/s)
     2 threads, 64 streams: 9.04 GiB in 10.003 s (925.61 MiB/s)
     4 threads, 64 streams: 17.50 GiB in 10.002 s (1.75 GiB/s)
     8 threads, 64 streams: 34.48 GiB in 10.001 s (3.45 GiB/s)
    16 threads, 64 streams: 60.93 GiB in 10.001 s (6.09 GiB/s)
    32 threads, 64 streams: 59.37 GiB in 10.001 s (5.94 GiB/s)
    64 threads, 64 streams: 112.61 GiB in 10.001 s (11.26 GiB/s)

## The bench-zero.py script

This script compare the throughput of `blkhash` for hashing data which is
all zeros with different number of threads.

Example run of the benchmark script:

    $ test/bench-zero.py

    blkhash-bench --digest-name sha256 --input-type zero

     1 threads, 64 streams: 250.29 GiB in 10.075 s (24.84 GiB/s)
     2 threads, 64 streams: 252.90 GiB in 10.051 s (25.16 GiB/s)
     4 threads, 64 streams: 255.56 GiB in 10.030 s (25.48 GiB/s)
     8 threads, 64 streams: 254.89 GiB in 10.016 s (25.45 GiB/s)
    16 threads, 64 streams: 254.87 GiB in 10.010 s (25.46 GiB/s)
    32 threads, 64 streams: 255.17 GiB in 10.008 s (25.50 GiB/s)
    64 threads, 64 streams: 250.43 GiB in 10.006 s (25.03 GiB/s)

    blkhash-bench --digest-name sha256 --input-type zero --aio

     1 threads, 64 streams: 155.86 GiB in 10.000 s (15.59 GiB/s)
     2 threads, 64 streams: 357.57 GiB in 10.000 s (35.76 GiB/s)
     4 threads, 64 streams: 976.77 GiB in 10.000 s (97.67 GiB/s)
     8 threads, 64 streams: 440.83 GiB in 10.000 s (44.08 GiB/s)
    16 threads, 64 streams: 403.21 GiB in 10.000 s (40.32 GiB/s)
    32 threads, 64 streams: 394.03 GiB in 10.000 s (39.40 GiB/s)
    64 threads, 64 streams: 374.01 GiB in 10.001 s (37.40 GiB/s)

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

## The bench-digest.py script

This script compare the throughput of `sha256` for hashing data with
different number of threads. This shows the highest possible throughput
on the machine, for evaluating `blkhash` efficiency.

Example run of the benchmark script:

$ test/bench-digest.py

    digest-bench --digest-name sha256

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

     1 threads, 64 streams: 157.63 GiB in 10.000 s (15.76 GiB/s)
     2 threads, 64 streams: 149.15 GiB in 10.000 s (14.91 GiB/s)
     4 threads, 64 streams: 149.73 GiB in 10.000 s (14.97 GiB/s)
     8 threads, 64 streams: 148.60 GiB in 10.000 s (14.86 GiB/s)
    16 threads, 64 streams: 121.97 GiB in 10.000 s (12.20 GiB/s)
    32 threads, 64 streams: 133.19 GiB in 10.000 s (13.32 GiB/s)
    64 threads, 64 streams: 131.25 GiB in 10.001 s (13.12 GiB/s)

    blkhash-bench --digest-name null --input-type data --aio

     1 threads, 64 streams: 1.03 TiB in 10.000 s (105.29 GiB/s)
     2 threads, 64 streams: 797.70 GiB in 10.000 s (79.77 GiB/s)
     4 threads, 64 streams: 421.87 GiB in 10.000 s (42.19 GiB/s)
     8 threads, 64 streams: 437.94 GiB in 10.000 s (43.79 GiB/s)
    16 threads, 64 streams: 392.91 GiB in 10.000 s (39.29 GiB/s)
    32 threads, 64 streams: 398.70 GiB in 10.000 s (39.87 GiB/s)
    64 threads, 64 streams: 394.66 GiB in 10.001 s (39.46 GiB/s)

    blkhash-bench --digest-name null --input-type zero

     1 threads, 64 streams: 255.26 GiB in 10.003 s (25.52 GiB/s)
     2 threads, 64 streams: 250.43 GiB in 10.004 s (25.03 GiB/s)
     4 threads, 64 streams: 253.80 GiB in 10.003 s (25.37 GiB/s)
     8 threads, 64 streams: 255.38 GiB in 10.002 s (25.53 GiB/s)
    16 threads, 64 streams: 256.14 GiB in 10.000 s (25.61 GiB/s)
    32 threads, 64 streams: 251.99 GiB in 10.002 s (25.19 GiB/s)
    64 threads, 64 streams: 256.02 GiB in 10.001 s (25.60 GiB/s)

    blkhash-bench --digest-name null --input-type zero --aio

     1 threads, 64 streams: 155.10 GiB in 10.000 s (15.51 GiB/s)
     2 threads, 64 streams: 375.25 GiB in 10.000 s (37.52 GiB/s)
     4 threads, 64 streams: 980.70 GiB in 10.000 s (98.07 GiB/s)
     8 threads, 64 streams: 438.21 GiB in 10.000 s (43.82 GiB/s)
    16 threads, 64 streams: 407.92 GiB in 10.000 s (40.79 GiB/s)
    32 threads, 64 streams: 388.47 GiB in 10.000 s (38.85 GiB/s)
    64 threads, 64 streams: 385.57 GiB in 10.001 s (38.55 GiB/s)

    blkhash-bench --digest-name null --input-type hole

     1 threads, 64 streams: 213.25 TiB in 10.004 s (21.32 TiB/s)
     2 threads, 64 streams: 430.00 TiB in 10.001 s (43.00 TiB/s)
     4 threads, 64 streams: 539.50 TiB in 10.001 s (53.94 TiB/s)
     8 threads, 64 streams: 514.69 TiB in 10.002 s (51.46 TiB/s)
    16 threads, 64 streams: 706.50 TiB in 10.002 s (70.64 TiB/s)
    32 threads, 64 streams: 965.50 TiB in 10.002 s (96.53 TiB/s)
    64 threads, 64 streams: 644.56 TiB in 10.002 s (64.44 TiB/s)

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


## The cache program

This is a helper to load image data into the page cache or to drop image
from the page cache.

To load image data into the page cache:

    $ build/test/cache /data/tmp/blksum/80p.raw
    10.000 GiB in 4.678 seconds (2.138 GiB/s)

A second run will be much faster now:

    $ build/test/cache /data/tmp/blksum/80p.raw
    10.000 GiB in 0.786 seconds (12.716 GiB/s)

To drop image data from the page cache:

    $ build/test/cache --drop /data/tmp/blksum/80p.raw
