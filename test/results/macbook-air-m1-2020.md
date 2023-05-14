<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Test results on Apple MacBook Air M1 2020

## The environment

Info from `System Information` app:

    Hardware Overview:

      Model Name:	MacBook Air
      Model Identifier:	MacBookAir10,1
      Model Number:	MGND3HB/A
      Chip:	Apple M1
      Total Number of Cores:	8 (4 performance and 4 efficiency)
      Memory:	8 GB
      System Firmware Version:	8419.41.10
      OS Loader Version:	8419.41.10

I don't how to disable frequency scaling, but results seems to be very
consistent and I did experience any heating issues, so this is likely
not needed.

## Testing with defaults

Tests results:

```
 % test/bench-data.py; test/bench-zero.py; test/bench-hole.py; test/bench-null.py

blkhash-bench --digest-name sha256 --input-type data

 1 threads, 64 streams: 4.21 GiB in 2.006 s (2.10 GiB/s)
 2 threads, 64 streams: 8.20 GiB in 2.006 s (4.09 GiB/s)
 4 threads, 64 streams: 15.62 GiB in 2.004 s (7.79 GiB/s)
 8 threads, 64 streams: 22.36 GiB in 2.005 s (11.15 GiB/s)

blkhash-bench --digest-name sha256 --input-type data --aio

 1 threads, 64 streams: 4.24 GiB in 2.002 s (2.12 GiB/s)
 2 threads, 64 streams: 8.24 GiB in 2.006 s (4.11 GiB/s)
 4 threads, 64 streams: 15.33 GiB in 2.005 s (7.64 GiB/s)
 8 threads, 64 streams: 22.86 GiB in 2.001 s (11.42 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero

 1 threads, 64 streams: 70.67 GiB in 2.007 s (35.22 GiB/s)
 2 threads, 64 streams: 70.80 GiB in 2.003 s (35.36 GiB/s)
 4 threads, 64 streams: 70.95 GiB in 2.006 s (35.38 GiB/s)
 8 threads, 64 streams: 70.97 GiB in 2.006 s (35.39 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero --aio

 1 threads, 64 streams: 46.28 GiB in 2.001 s (23.13 GiB/s)
 2 threads, 64 streams: 100.84 GiB in 2.002 s (50.38 GiB/s)
 4 threads, 64 streams: 147.85 GiB in 2.005 s (73.74 GiB/s)
 8 threads, 64 streams: 185.24 GiB in 2.003 s (92.48 GiB/s)

blkhash-bench --digest-name sha256 --input-type hole

 1 threads, 64 streams: 7.81 TiB in 2.019 s (3.87 TiB/s)
 2 threads, 64 streams: 15.12 TiB in 2.012 s (7.52 TiB/s)
 4 threads, 64 streams: 28.88 TiB in 2.008 s (14.38 TiB/s)
 8 threads, 64 streams: 39.06 TiB in 2.011 s (19.43 TiB/s)

blkhash-bench --digest-name null --input-type data

 1 threads, 64 streams: 57.65 GiB in 2.001 s (28.81 GiB/s)
 2 threads, 64 streams: 43.67 GiB in 2.005 s (21.78 GiB/s)
 4 threads, 64 streams: 32.56 GiB in 2.003 s (16.26 GiB/s)
 8 threads, 64 streams: 37.63 GiB in 2.001 s (18.81 GiB/s)

blkhash-bench --digest-name null --input-type data --aio

 1 threads, 64 streams: 454.70 GiB in 2.005 s (226.78 GiB/s)
 2 threads, 64 streams: 487.78 GiB in 2.005 s (243.28 GiB/s)
 4 threads, 64 streams: 270.21 GiB in 2.005 s (134.77 GiB/s)
 8 threads, 64 streams: 124.29 GiB in 2.005 s (61.99 GiB/s)

blkhash-bench --digest-name null --input-type zero

 1 threads, 64 streams: 70.31 GiB in 2.005 s (35.06 GiB/s)
 2 threads, 64 streams: 70.06 GiB in 2.005 s (34.95 GiB/s)
 4 threads, 64 streams: 70.04 GiB in 2.005 s (34.93 GiB/s)
 8 threads, 64 streams: 70.38 GiB in 2.005 s (35.10 GiB/s)

blkhash-bench --digest-name null --input-type zero --aio

 1 threads, 64 streams: 47.93 GiB in 2.005 s (23.90 GiB/s)
 2 threads, 64 streams: 101.10 GiB in 2.002 s (50.51 GiB/s)
 4 threads, 64 streams: 147.30 GiB in 2.005 s (73.46 GiB/s)
 8 threads, 64 streams: 182.73 GiB in 2.002 s (91.27 GiB/s)

blkhash-bench --digest-name null --input-type hole

 1 threads, 64 streams: 59.06 TiB in 2.002 s (29.50 TiB/s)
 2 threads, 64 streams: 115.06 TiB in 2.011 s (57.21 TiB/s)
 4 threads, 64 streams: 217.25 TiB in 2.011 s (108.03 TiB/s)
 8 threads, 64 streams: 341.44 TiB in 2.008 s (170.06 TiB/s)
```
