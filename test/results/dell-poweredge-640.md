<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Test results on Dell PowerEdge 640

## Test environment

Using latency-performance profile for more consistent results.

    $ tuned-adm profile latency-performance

    $ cpupower frequency-info
    analyzing CPU 0:
      driver: intel_cpufreq
      CPUs which run at the same hardware frequency: 0
      CPUs which need to have their frequency coordinated by software: 0
      maximum transition latency: 20.0 us
      hardware limits: 800 MHz - 4.00 GHz
      available cpufreq governors: conservative ondemand userspace powersave performance schedutil
      current policy: frequency should be within 4.00 GHz and 4.00 GHz.
                      The governor "performance" may decide which speed to use
                      within this range.
      current CPU frequency: Unable to call hardware
      current CPU frequency: 2.90 GHz (asserted by call to kernel)
      boost state support:
        Supported: yes
        Active: yes

    $ cpupower frequency-info -o
              minimum CPU frequency  -  maximum CPU frequency  -  governor
    CPU  0      4000000 kHz (100 %)  -    4000000 kHz (100 %)  -  performance
    CPU  1      4000000 kHz (100 %)  -    4000000 kHz (100 %)  -  performance
    CPU  2      4000000 kHz (100 %)  -    4000000 kHz (100 %)  -  performance
    ...
    CPU 79      4000000 kHz (100 %)  -    4000000 kHz (100 %)  -  performance

## blkhash-bench

Benchmark script was modified manually to run for 10 seconds without
cool down wait.

```
$ test/bench-data.py; test/bench-zero.py; test/bench-hole.py; test/bench-null.py

blkhash-bench --digest-name sha256 --input-type data

 1 threads, 64 streams: 3.46 GiB in 10.004 s (354.67 MiB/s)
 2 threads, 64 streams: 6.92 GiB in 10.003 s (708.23 MiB/s)
 4 threads, 64 streams: 13.82 GiB in 10.003 s (1.38 GiB/s)
 8 threads, 64 streams: 27.39 GiB in 10.003 s (2.74 GiB/s)
16 threads, 64 streams: 43.43 GiB in 10.001 s (4.34 GiB/s)
32 threads, 64 streams: 39.40 GiB in 10.001 s (3.94 GiB/s)
64 threads, 64 streams: 59.21 GiB in 10.001 s (5.92 GiB/s)

blkhash-bench --digest-name sha256 --input-type data --aio

 1 threads, 64 streams: 3.46 GiB in 10.003 s (354.51 MiB/s)
 2 threads, 64 streams: 6.93 GiB in 10.003 s (709.00 MiB/s)
 4 threads, 64 streams: 13.82 GiB in 10.003 s (1.38 GiB/s)
 8 threads, 64 streams: 27.48 GiB in 10.001 s (2.75 GiB/s)
16 threads, 64 streams: 54.58 GiB in 10.001 s (5.46 GiB/s)
32 threads, 64 streams: 59.49 GiB in 10.001 s (5.95 GiB/s)
64 threads, 64 streams: 106.42 GiB in 10.001 s (10.64 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero

 1 threads, 64 streams: 229.57 GiB in 10.066 s (22.81 GiB/s)
 2 threads, 64 streams: 231.55 GiB in 10.042 s (23.06 GiB/s)
 4 threads, 64 streams: 230.76 GiB in 10.022 s (23.03 GiB/s)
 8 threads, 64 streams: 233.83 GiB in 10.013 s (23.35 GiB/s)
16 threads, 64 streams: 233.84 GiB in 10.008 s (23.36 GiB/s)
32 threads, 64 streams: 230.73 GiB in 10.006 s (23.06 GiB/s)
64 threads, 64 streams: 233.69 GiB in 10.006 s (23.36 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero --aio

 1 threads, 64 streams: 142.03 GiB in 10.000 s (14.20 GiB/s)
 2 threads, 64 streams: 303.67 GiB in 10.000 s (30.37 GiB/s)
 4 threads, 64 streams: 806.20 GiB in 10.000 s (80.62 GiB/s)
 8 threads, 64 streams: 388.39 GiB in 10.000 s (38.84 GiB/s)
16 threads, 64 streams: 394.74 GiB in 10.000 s (39.47 GiB/s)
32 threads, 64 streams: 384.77 GiB in 10.000 s (38.48 GiB/s)
64 threads, 64 streams: 375.72 GiB in 10.001 s (37.57 GiB/s)

blkhash-bench --digest-name sha256 --input-type hole

 1 threads, 64 streams: 5.62 TiB in 10.069 s (572.04 GiB/s)
 2 threads, 64 streams: 9.31 TiB in 10.044 s (949.41 GiB/s)
 4 threads, 64 streams: 21.19 TiB in 10.054 s (2.11 TiB/s)
 8 threads, 64 streams: 26.19 TiB in 10.053 s (2.60 TiB/s)
16 threads, 64 streams: 56.38 TiB in 10.037 s (5.62 TiB/s)
32 threads, 64 streams: 76.94 TiB in 10.035 s (7.67 TiB/s)
64 threads, 64 streams: 100.56 TiB in 10.038 s (10.02 TiB/s)

blkhash-bench --digest-name null --input-type data

 1 threads, 64 streams: 130.55 GiB in 10.000 s (13.06 GiB/s)
 2 threads, 64 streams: 130.82 GiB in 10.000 s (13.08 GiB/s)
 4 threads, 64 streams: 133.27 GiB in 10.000 s (13.33 GiB/s)
 8 threads, 64 streams: 130.60 GiB in 10.000 s (13.06 GiB/s)
16 threads, 64 streams: 133.63 GiB in 10.000 s (13.36 GiB/s)
32 threads, 64 streams: 131.37 GiB in 10.000 s (13.14 GiB/s)
64 threads, 64 streams: 132.30 GiB in 10.001 s (13.23 GiB/s)

blkhash-bench --digest-name null --input-type data --aio

 1 threads, 64 streams: 1.04 TiB in 10.000 s (106.75 GiB/s)
 2 threads, 64 streams: 794.94 GiB in 10.000 s (79.49 GiB/s)
 4 threads, 64 streams: 380.84 GiB in 10.000 s (38.08 GiB/s)
 8 threads, 64 streams: 404.24 GiB in 10.000 s (40.42 GiB/s)
16 threads, 64 streams: 402.53 GiB in 10.000 s (40.25 GiB/s)
32 threads, 64 streams: 398.45 GiB in 10.000 s (39.84 GiB/s)
64 threads, 64 streams: 393.12 GiB in 10.001 s (39.31 GiB/s)

blkhash-bench --digest-name null --input-type zero

 1 threads, 64 streams: 230.40 GiB in 10.002 s (23.03 GiB/s)
 2 threads, 64 streams: 233.60 GiB in 10.004 s (23.35 GiB/s)
 4 threads, 64 streams: 233.94 GiB in 10.002 s (23.39 GiB/s)
 8 threads, 64 streams: 233.80 GiB in 10.001 s (23.38 GiB/s)
16 threads, 64 streams: 233.82 GiB in 10.001 s (23.38 GiB/s)
32 threads, 64 streams: 233.79 GiB in 10.001 s (23.38 GiB/s)
64 threads, 64 streams: 229.21 GiB in 10.001 s (22.92 GiB/s)

blkhash-bench --digest-name null --input-type zero --aio

 1 threads, 64 streams: 146.41 GiB in 10.000 s (14.64 GiB/s)
 2 threads, 64 streams: 318.72 GiB in 10.000 s (31.87 GiB/s)
 4 threads, 64 streams: 862.37 GiB in 10.000 s (86.24 GiB/s)
 8 threads, 64 streams: 395.48 GiB in 10.000 s (39.55 GiB/s)
16 threads, 64 streams: 399.09 GiB in 10.000 s (39.91 GiB/s)
32 threads, 64 streams: 381.80 GiB in 10.000 s (38.18 GiB/s)
64 threads, 64 streams: 384.04 GiB in 10.001 s (38.40 GiB/s)

blkhash-bench --digest-name null --input-type hole

 1 threads, 64 streams: 164.25 TiB in 10.002 s (16.42 TiB/s)
 2 threads, 64 streams: 330.06 TiB in 10.002 s (33.00 TiB/s)
 4 threads, 64 streams: 270.75 TiB in 10.002 s (27.07 TiB/s)
 8 threads, 64 streams: 484.81 TiB in 10.003 s (48.47 TiB/s)
16 threads, 64 streams: 470.81 TiB in 10.002 s (47.07 TiB/s)
32 threads, 64 streams: 699.19 TiB in 10.002 s (69.91 TiB/s)
64 threads, 64 streams: 614.94 TiB in 10.003 s (61.48 TiB/s)
```

## blksum - empty image

```
$ hyperfine -p "sleep 1" -L t 1,2,4,8,16,32 "build/bin/blksum -t {t} -c empty-4t.raw"
Benchmark 1: build/bin/blksum -t 1 -c empty-4t.raw
  Time (mean ± σ):      7.176 s ±  0.020 s    [User: 7.196 s, System: 0.060 s]
  Range (min … max):    7.159 s …  7.230 s    10 runs

Benchmark 2: build/bin/blksum -t 2 -c empty-4t.raw
  Time (mean ± σ):      3.891 s ±  0.385 s    [User: 7.789 s, System: 0.057 s]
  Range (min … max):    3.584 s …  4.370 s    10 runs

Benchmark 3: build/bin/blksum -t 4 -c empty-4t.raw
  Time (mean ± σ):      1.817 s ±  0.023 s    [User: 7.241 s, System: 0.050 s]
  Range (min … max):    1.802 s …  1.879 s    10 runs

Benchmark 4: build/bin/blksum -t 8 -c empty-4t.raw
  Time (mean ± σ):      1.007 s ±  0.106 s    [User: 7.837 s, System: 0.051 s]
  Range (min … max):    0.904 s …  1.143 s    10 runs

Benchmark 5: build/bin/blksum -t 16 -c empty-4t.raw
  Time (mean ± σ):     612.2 ms ± 161.0 ms    [User: 8779.2 ms, System: 49.6 ms]
  Range (min … max):   467.5 ms … 1007.1 ms    10 runs

Benchmark 6: build/bin/blksum -t 32 -c empty-4t.raw
  Time (mean ± σ):      1.185 s ±  0.407 s    [User: 33.621 s, System: 0.070 s]
  Range (min … max):    0.602 s …  1.546 s    10 runs

Summary
  'build/bin/blksum -t 16 -c empty-4t.raw' ran
    1.64 ± 0.47 times faster than 'build/bin/blksum -t 8 -c empty-4t.raw'
    1.93 ± 0.84 times faster than 'build/bin/blksum -t 32 -c empty-4t.raw'
    2.97 ± 0.78 times faster than 'build/bin/blksum -t 4 -c empty-4t.raw'
    6.35 ± 1.79 times faster than 'build/bin/blksum -t 2 -c empty-4t.raw'
   11.72 ± 3.08 times faster than 'build/bin/blksum -t 1 -c empty-4t.raw'
```

## blksum - 25% full image

```
$ hyperfine -p "sleep 1" -L t 1,2,4,8,16,32 "build/bin/blksum -t {t} -c 25p.raw"
Benchmark 1: build/bin/blksum -t 1 -c 25p.raw
  Time (mean ± σ):      3.953 s ±  0.004 s    [User: 4.146 s, System: 0.874 s]
  Range (min … max):    3.945 s …  3.957 s    10 runs

Benchmark 2: build/bin/blksum -t 2 -c 25p.raw
  Time (mean ± σ):      1.985 s ±  0.002 s    [User: 4.134 s, System: 0.824 s]
  Range (min … max):    1.982 s …  1.988 s    10 runs

Benchmark 3: build/bin/blksum -t 4 -c 25p.raw
  Time (mean ± σ):     997.4 ms ±   1.9 ms    [User: 4110.3 ms, System: 819.5 ms]
  Range (min … max):   995.0 ms … 1001.2 ms    10 runs

Benchmark 4: build/bin/blksum -t 8 -c 25p.raw
  Time (mean ± σ):     510.4 ms ±  21.5 ms    [User: 4115.5 ms, System: 942.5 ms]
  Range (min … max):   502.3 ms … 571.7 ms    10 runs

Benchmark 5: build/bin/blksum -t 16 -c 25p.raw
  Time (mean ± σ):     436.1 ms ±  13.6 ms    [User: 4291.2 ms, System: 1062.5 ms]
  Range (min … max):   407.7 ms … 451.1 ms    10 runs

Benchmark 6: build/bin/blksum -t 32 -c 25p.raw
  Time (mean ± σ):     421.1 ms ±  25.9 ms    [User: 5564.9 ms, System: 1045.7 ms]
  Range (min … max):   379.6 ms … 457.6 ms    10 runs

Summary
  'build/bin/blksum -t 32 -c 25p.raw' ran
    1.04 ± 0.07 times faster than 'build/bin/blksum -t 16 -c 25p.raw'
    1.21 ± 0.09 times faster than 'build/bin/blksum -t 8 -c 25p.raw'
    2.37 ± 0.15 times faster than 'build/bin/blksum -t 4 -c 25p.raw'
    4.71 ± 0.29 times faster than 'build/bin/blksum -t 2 -c 25p.raw'
    9.39 ± 0.58 times faster than 'build/bin/blksum -t 1 -c 25p.raw'
```

## blksum - 50% full image

```
$ hyperfine -p "sleep 1" -L t 1,2,4,8,16,32 "build/bin/blksum -t {t} -c 50p.raw"
Benchmark 1: build/bin/blksum -t 1 -c 50p.raw
  Time (mean ± σ):      8.193 s ±  0.003 s    [User: 8.668 s, System: 1.787 s]
  Range (min … max):    8.188 s …  8.197 s    10 runs

Benchmark 2: build/bin/blksum -t 2 -c 50p.raw
  Time (mean ± σ):      4.111 s ±  0.003 s    [User: 8.618 s, System: 1.735 s]
  Range (min … max):    4.105 s …  4.114 s    10 runs

Benchmark 3: build/bin/blksum -t 4 -c 50p.raw
  Time (mean ± σ):      2.061 s ±  0.003 s    [User: 8.602 s, System: 1.755 s]
  Range (min … max):    2.056 s …  2.066 s    10 runs

Benchmark 4: build/bin/blksum -t 8 -c 50p.raw
  Time (mean ± σ):      1.081 s ±  0.084 s    [User: 8.620 s, System: 1.949 s]
  Range (min … max):    1.031 s …  1.283 s    10 runs

Benchmark 5: build/bin/blksum -t 16 -c 50p.raw
  Time (mean ± σ):     924.7 ms ± 150.2 ms    [User: 9107.4 ms, System: 2010.4 ms]
  Range (min … max):   843.7 ms … 1345.8 ms    10 runs

Benchmark 6: build/bin/blksum -t 32 -c 50p.raw
  Time (mean ± σ):     997.8 ms ± 201.5 ms    [User: 10812.6 ms, System: 2215.1 ms]
  Range (min … max):   882.1 ms … 1511.4 ms    10 runs

Summary
  'build/bin/blksum -t 16 -c 50p.raw' ran
    1.08 ± 0.28 times faster than 'build/bin/blksum -t 32 -c 50p.raw'
    1.17 ± 0.21 times faster than 'build/bin/blksum -t 8 -c 50p.raw'
    2.23 ± 0.36 times faster than 'build/bin/blksum -t 4 -c 50p.raw'
    4.45 ± 0.72 times faster than 'build/bin/blksum -t 2 -c 50p.raw'
    8.86 ± 1.44 times faster than 'build/bin/blksum -t 1 -c 50p.raw'
```

## blksum - 100% image

```
$ hyperfine -p "sleep 1" -L t 1,2,4,8,16,32 "build/bin/blksum -t {t} -c full-6g.raw"
Benchmark 1: build/bin/blksum -t 1 -c full-6g.raw
  Time (mean ± σ):     17.347 s ±  0.006 s    [User: 18.198 s, System: 3.622 s]
  Range (min … max):   17.339 s … 17.357 s    10 runs

Benchmark 2: build/bin/blksum -t 2 -c full-6g.raw
  Time (mean ± σ):      8.693 s ±  0.002 s    [User: 18.164 s, System: 3.561 s]
  Range (min … max):    8.688 s …  8.695 s    10 runs

Benchmark 3: build/bin/blksum -t 4 -c full-6g.raw
  Time (mean ± σ):      4.352 s ±  0.003 s    [User: 18.083 s, System: 3.566 s]
  Range (min … max):    4.344 s …  4.354 s    10 runs

Benchmark 4: build/bin/blksum -t 8 -c full-6g.raw
  Time (mean ± σ):      2.375 s ±  0.195 s    [User: 18.549 s, System: 4.297 s]
  Range (min … max):    2.176 s …  2.706 s    10 runs

Benchmark 5: build/bin/blksum -t 16 -c full-6g.raw
  Time (mean ± σ):      1.890 s ±  0.101 s    [User: 18.504 s, System: 4.687 s]
  Range (min … max):    1.749 s …  2.058 s    10 runs

Benchmark 6: build/bin/blksum -t 32 -c full-6g.raw
  Time (mean ± σ):      1.965 s ±  0.268 s    [User: 23.091 s, System: 4.647 s]
  Range (min … max):    1.644 s …  2.574 s    10 runs

Summary
  'build/bin/blksum -t 16 -c full-6g.raw' ran
    1.04 ± 0.15 times faster than 'build/bin/blksum -t 32 -c full-6g.raw'
    1.26 ± 0.12 times faster than 'build/bin/blksum -t 8 -c full-6g.raw'
    2.30 ± 0.12 times faster than 'build/bin/blksum -t 4 -c full-6g.raw'
    4.60 ± 0.25 times faster than 'build/bin/blksum -t 2 -c full-6g.raw'
    9.18 ± 0.49 times faster than 'build/bin/blksum -t 1 -c full-6g.raw'
```
