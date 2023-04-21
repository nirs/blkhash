<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Experimenting with --queue-depth

The `--queue-depth` determine the number of concurrent in flight reads
or updates. Having multiple concurrent reads improves I/O throughput and
having multiple hash updates improves blkhash throughput.

I tested this on 2 systems:
- Lenovo ThinkPad P1 Gen 3 (Laptop)
- Dell PowerEdge R640 (Server)

Both using performance CPU governor and all cores (including
hyper-threading). To mitigate over heating, sleep for a while between
the runs.

## NBD disabled

When NBD is not available or disabled, we use synchronous I/O, but we
still have multiple in flight hash updates.

Building:

```
meson configure build -Dnbd=disabled
meson compile -C build
```

### Laptop --digest-name null

This tests the maximum throughput reading data from the page cache and
pushing it into blkhash.

```
$ hyperfine -p 'sleep 3' -L q 1,2,4,8,16,32 "build/bin/blksum -d null --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -d null --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):     762.6 ms ±  21.8 ms    [User: 134.0 ms, System: 812.6 ms]
  Range (min … max):   726.6 ms … 808.3 ms    10 runs

Benchmark 2: build/bin/blksum -d null --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):     639.2 ms ±  12.9 ms    [User: 97.4 ms, System: 704.0 ms]
  Range (min … max):   625.3 ms … 662.2 ms    10 runs

Benchmark 3: build/bin/blksum -d null --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):     590.3 ms ±  13.5 ms    [User: 73.5 ms, System: 651.7 ms]
  Range (min … max):   576.7 ms … 611.9 ms    10 runs

Benchmark 4: build/bin/blksum -d null --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):     583.9 ms ±  10.3 ms    [User: 61.5 ms, System: 620.0 ms]
  Range (min … max):   569.0 ms … 596.3 ms    10 runs

Benchmark 5: build/bin/blksum -d null --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):     592.7 ms ±   9.3 ms    [User: 61.5 ms, System: 624.7 ms]
  Range (min … max):   581.2 ms … 609.9 ms    10 runs

Benchmark 6: build/bin/blksum -d null --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):     671.8 ms ±  16.7 ms    [User: 67.2 ms, System: 691.9 ms]
  Range (min … max):   644.5 ms … 695.3 ms    10 runs

Summary
  'build/bin/blksum -d null --queue-depth 8 -c full-6g.raw' ran
    1.01 ± 0.03 times faster than 'build/bin/blksum -d null --queue-depth 4 -c full-6g.raw'
    1.02 ± 0.02 times faster than 'build/bin/blksum -d null --queue-depth 16 -c full-6g.raw'
    1.09 ± 0.03 times faster than 'build/bin/blksum -d null --queue-depth 2 -c full-6g.raw'
    1.15 ± 0.04 times faster than 'build/bin/blksum -d null --queue-depth 32 -c full-6g.raw'
    1.31 ± 0.04 times faster than 'build/bin/blksum -d null --queue-depth 1 -c full-6g.raw'
```

### Laptop --digest-name sha256

For this test we use 16 threads which gives the best throughput.

```
$ hyperfine -p 'sleep 10' -L q 1,2,4,8,16,32 "build/bin/blksum -t 16 --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      4.130 s ±  0.187 s    [User: 12.379 s, System: 0.904 s]
  Range (min … max):    3.924 s …  4.492 s    10 runs

Benchmark 2: build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      3.363 s ±  0.048 s    [User: 18.572 s, System: 0.891 s]
  Range (min … max):    3.290 s …  3.441 s    10 runs

Benchmark 3: build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):      2.569 s ±  0.028 s    [User: 22.073 s, System: 1.240 s]
  Range (min … max):    2.544 s …  2.634 s    10 runs

Benchmark 4: build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):      2.341 s ±  0.020 s    [User: 23.267 s, System: 1.110 s]
  Range (min … max):    2.326 s …  2.391 s    10 runs

Benchmark 5: build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      2.235 s ±  0.036 s    [User: 23.994 s, System: 1.030 s]
  Range (min … max):    2.209 s …  2.334 s    10 runs

Benchmark 6: build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      2.230 s ±  0.046 s    [User: 24.172 s, System: 0.970 s]
  Range (min … max):    2.199 s …  2.328 s    10 runs

Summary
  'build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw' ran
    1.00 ± 0.03 times faster than 'build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw'
    1.05 ± 0.02 times faster than 'build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw'
    1.15 ± 0.03 times faster than 'build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw'
    1.51 ± 0.04 times faster than 'build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw'
    1.85 ± 0.09 times faster than 'build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw'
```

### Server - --digest-name null

```
$ hyperfine -L q 1,2,4,8,16,32 "build/bin/blksum -d null --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -d null --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      1.262 s ±  0.003 s    [User: 0.185 s, System: 1.410 s]
  Range (min … max):    1.258 s …  1.266 s    10 runs

Benchmark 2: build/bin/blksum -d null --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      1.131 s ±  0.003 s    [User: 0.184 s, System: 1.334 s]
  Range (min … max):    1.128 s …  1.136 s    10 runs

Benchmark 3: build/bin/blksum -d null --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):      1.177 s ±  0.008 s    [User: 0.172 s, System: 1.395 s]
  Range (min … max):    1.155 s …  1.185 s    10 runs

Benchmark 4: build/bin/blksum -d null --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):      1.162 s ±  0.015 s    [User: 0.161 s, System: 1.390 s]
  Range (min … max):    1.147 s …  1.178 s    10 runs

Benchmark 5: build/bin/blksum -d null --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      1.159 s ±  0.015 s    [User: 0.151 s, System: 1.333 s]
  Range (min … max):    1.135 s …  1.180 s    10 runs

Benchmark 6: build/bin/blksum -d null --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      1.168 s ±  0.010 s    [User: 0.147 s, System: 1.344 s]
  Range (min … max):    1.156 s …  1.180 s    10 runs

Summary
  'build/bin/blksum -d null --queue-depth 2 -c full-6g.raw' ran
    1.03 ± 0.01 times faster than 'build/bin/blksum -d null --queue-depth 16 -c full-6g.raw'
    1.03 ± 0.01 times faster than 'build/bin/blksum -d null --queue-depth 8 -c full-6g.raw'
    1.03 ± 0.01 times faster than 'build/bin/blksum -d null --queue-depth 32 -c full-6g.raw'
    1.04 ± 0.01 times faster than 'build/bin/blksum -d null --queue-depth 4 -c full-6g.raw'
    1.12 ± 0.00 times faster than 'build/bin/blksum -d null --queue-depth 1 -c full-6g.raw'
```

### Server - --digest-name sha256

```
$ hyperfine -L q 1,2,4,8,16,32 "build/bin/blksum -t 16 --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      6.332 s ±  0.008 s    [User: 19.961 s, System: 1.446 s]
  Range (min … max):    6.321 s …  6.347 s    10 runs

Benchmark 2: build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      2.942 s ±  0.139 s    [User: 17.486 s, System: 1.547 s]
  Range (min … max):    2.847 s …  3.318 s    10 runs

Benchmark 3: build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):      1.589 s ±  0.117 s    [User: 17.511 s, System: 1.573 s]
  Range (min … max):    1.494 s …  1.844 s    10 runs

Benchmark 4: build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):      1.397 s ±  0.149 s    [User: 17.451 s, System: 1.506 s]
  Range (min … max):    1.257 s …  1.721 s    10 runs

Benchmark 5: build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      1.252 s ±  0.051 s    [User: 17.377 s, System: 1.269 s]
  Range (min … max):    1.198 s …  1.342 s    10 runs

Benchmark 6: build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      1.297 s ±  0.093 s    [User: 17.332 s, System: 1.306 s]
  Range (min … max):    1.211 s …  1.462 s    10 runs

Summary
  'build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw' ran
    1.04 ± 0.09 times faster than 'build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw'
    1.12 ± 0.13 times faster than 'build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw'
    1.27 ± 0.11 times faster than 'build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw'
    2.35 ± 0.15 times faster than 'build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw'
    5.06 ± 0.21 times faster than 'build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw'
```

## Using NBD

If NBD is available we always use it since it supports detection of
unallocated areas. However the read throughput is lower compared with
directly reading from the file.

Building:

```
meson configure build -Dnbd=enabled
meson compile -C build
```

### Laptop - --digest-name null

Tests the maximum throughput copying image data from the underlying NBD
server.

```
$ hyperfine -p 'sleep 6' -L q 1,2,4,8,16,32 "build/bin/blksum -d null --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -d null --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      1.918 s ±  0.030 s    [User: 0.501 s, System: 2.260 s]
  Range (min … max):    1.877 s …  1.967 s    10 runs

Benchmark 2: build/bin/blksum -d null --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      1.305 s ±  0.027 s    [User: 0.373 s, System: 2.346 s]
  Range (min … max):    1.248 s …  1.339 s    10 runs

Benchmark 3: build/bin/blksum -d null --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):     981.3 ms ±  17.7 ms    [User: 326.9 ms, System: 2615.7 ms]
  Range (min … max):   954.1 ms … 1009.4 ms    10 runs

Benchmark 4: build/bin/blksum -d null --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):     975.8 ms ±  20.8 ms    [User: 305.5 ms, System: 3636.1 ms]
  Range (min … max):   959.7 ms … 1028.5 ms    10 runs

Benchmark 5: build/bin/blksum -d null --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      1.430 s ±  0.167 s    [User: 0.410 s, System: 5.941 s]
  Range (min … max):    1.176 s …  1.599 s    10 runs

Benchmark 6: build/bin/blksum -d null --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      1.291 s ±  0.016 s    [User: 0.354 s, System: 4.665 s]
  Range (min … max):    1.268 s …  1.324 s    10 runs

Summary
  'build/bin/blksum -d null --queue-depth 8 -c full-6g.raw' ran
    1.01 ± 0.03 times faster than 'build/bin/blksum -d null --queue-depth 4 -c full-6g.raw'
    1.32 ± 0.03 times faster than 'build/bin/blksum -d null --queue-depth 32 -c full-6g.raw'
    1.34 ± 0.04 times faster than 'build/bin/blksum -d null --queue-depth 2 -c full-6g.raw'
    1.47 ± 0.17 times faster than 'build/bin/blksum -d null --queue-depth 16 -c full-6g.raw'
    1.97 ± 0.05 times faster than 'build/bin/blksum -d null --queue-depth 1 -c full-6g.raw'
```

### Laptop - --digest-name sha256

For this test we use 16 threads which gives the best throughput.

```
$ hyperfine -p 'sleep 15' -L q 1,2,4,8,16,32 "build/bin/blksum -t 16 --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      5.866 s ±  0.238 s    [User: 13.703 s, System: 2.480 s]
  Range (min … max):    5.359 s …  6.243 s    10 runs

Benchmark 2: build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      3.970 s ±  0.071 s    [User: 18.109 s, System: 2.622 s]
  Range (min … max):    3.873 s …  4.107 s    10 runs

Benchmark 3: build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):      2.869 s ±  0.024 s    [User: 21.642 s, System: 3.540 s]
  Range (min … max):    2.839 s …  2.923 s    10 runs

Benchmark 4: build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):      2.702 s ±  0.035 s    [User: 22.906 s, System: 3.936 s]
  Range (min … max):    2.677 s …  2.796 s    10 runs

Benchmark 5: build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      2.823 s ±  0.273 s    [User: 23.386 s, System: 5.021 s]
  Range (min … max):    2.640 s …  3.489 s    10 runs

Benchmark 6: build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      2.795 s ±  0.066 s    [User: 23.118 s, System: 5.810 s]
  Range (min … max):    2.733 s …  2.940 s    10 runs

Summary
  'build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw' ran
    1.03 ± 0.03 times faster than 'build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw'
    1.04 ± 0.10 times faster than 'build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw'
    1.06 ± 0.02 times faster than 'build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw'
    1.47 ± 0.03 times faster than 'build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw'
    2.17 ± 0.09 times faster than 'build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw'
```

### Server - --digest-name null

```
$ hyperfine -L q 1,2,4,8,16,32 "build/bin/blksum -d null --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -d null --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      3.120 s ±  0.012 s    [User: 0.670 s, System: 3.901 s]
  Range (min … max):    3.104 s …  3.139 s    10 runs

Benchmark 2: build/bin/blksum -d null --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      1.929 s ±  0.019 s    [User: 0.641 s, System: 3.751 s]
  Range (min … max):    1.901 s …  1.972 s    10 runs

Benchmark 3: build/bin/blksum -d null --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):      1.480 s ±  0.024 s    [User: 0.649 s, System: 3.636 s]
  Range (min … max):    1.446 s …  1.510 s    10 runs

Benchmark 4: build/bin/blksum -d null --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):      1.424 s ±  0.020 s    [User: 0.658 s, System: 3.701 s]
  Range (min … max):    1.399 s …  1.466 s    10 runs

Benchmark 5: build/bin/blksum -d null --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      1.437 s ±  0.015 s    [User: 0.674 s, System: 3.700 s]
  Range (min … max):    1.418 s …  1.461 s    10 runs

Benchmark 6: build/bin/blksum -d null --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      1.438 s ±  0.018 s    [User: 0.669 s, System: 3.622 s]
  Range (min … max):    1.418 s …  1.468 s    10 runs

Summary
  'build/bin/blksum -d null --queue-depth 8 -c full-6g.raw' ran
    1.01 ± 0.02 times faster than 'build/bin/blksum -d null --queue-depth 16 -c full-6g.raw'
    1.01 ± 0.02 times faster than 'build/bin/blksum -d null --queue-depth 32 -c full-6g.raw'
    1.04 ± 0.02 times faster than 'build/bin/blksum -d null --queue-depth 4 -c full-6g.raw'
    1.35 ± 0.02 times faster than 'build/bin/blksum -d null --queue-depth 2 -c full-6g.raw'
    2.19 ± 0.03 times faster than 'build/bin/blksum -d null --queue-depth 1 -c full-6g.raw'
```

### Server - --digest-name sha256

```
$ hyperfine -L q 1,2,4,8,16,32 "build/bin/blksum -t 16 --queue-depth {q} -c full-6g.raw"
Benchmark 1: build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw
  Time (mean ± σ):      8.425 s ±  0.088 s    [User: 20.644 s, System: 3.933 s]
  Range (min … max):    8.340 s …  8.583 s    10 runs

Benchmark 2: build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw
  Time (mean ± σ):      4.040 s ±  0.069 s    [User: 18.212 s, System: 3.977 s]
  Range (min … max):    3.953 s …  4.147 s    10 runs

Benchmark 3: build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw
  Time (mean ± σ):      2.722 s ±  0.345 s    [User: 18.920 s, System: 4.539 s]
  Range (min … max):    2.251 s …  3.097 s    10 runs

Benchmark 4: build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw
  Time (mean ± σ):      1.938 s ±  0.150 s    [User: 18.575 s, System: 4.342 s]
  Range (min … max):    1.749 s …  2.245 s    10 runs

Benchmark 5: build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw
  Time (mean ± σ):      1.943 s ±  0.213 s    [User: 18.596 s, System: 4.497 s]
  Range (min … max):    1.679 s …  2.447 s    10 runs

Benchmark 6: build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw
  Time (mean ± σ):      2.040 s ±  0.234 s    [User: 18.590 s, System: 4.727 s]
  Range (min … max):    1.794 s …  2.525 s    10 runs

Summary
  'build/bin/blksum -t 16 --queue-depth 8 -c full-6g.raw' ran
    1.00 ± 0.13 times faster than 'build/bin/blksum -t 16 --queue-depth 16 -c full-6g.raw'
    1.05 ± 0.15 times faster than 'build/bin/blksum -t 16 --queue-depth 32 -c full-6g.raw'
    1.40 ± 0.21 times faster than 'build/bin/blksum -t 16 --queue-depth 4 -c full-6g.raw'
    2.09 ± 0.16 times faster than 'build/bin/blksum -t 16 --queue-depth 2 -c full-6g.raw'
    4.35 ± 0.34 times faster than 'build/bin/blksum -t 16 --queue-depth 1 -c full-6g.raw'
```
