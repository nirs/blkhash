<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Async API performance

## Test environment

I tested this on 2 systems:
- Lenovo ThinkPad P1 Gen 3 (Laptop)
- Dell PowerEdge R640 (Server)

Both using latency-performance profile with tuned-adm and all cores
(including hyper-threading). To mitigate thermal throttling, sleep for a
while between the runs.

Testing 2 versions:
- blksum - current master
- blksum-async-api - async-api branch

## server - 25% full image

```
$ hyperfine -p "sleep 2" \
            "blkhash-async-api/build/bin/blksum -t 16 -c 25p.raw" \
            "blkhash/build/bin/blksum -t 16 -c 25p.raw"
Benchmark 1: blkhash-async-api/build/bin/blksum -t 16 -c 25p.raw
  Time (mean ± σ):     445.0 ms ±   8.2 ms    [User: 4329.9 ms, System: 1102.7 ms]
  Range (min … max):   430.6 ms … 454.9 ms    10 runs

Benchmark 2: blkhash/build/bin/blksum -t 16 -c 25p.raw
  Time (mean ± σ):     503.4 ms ±  24.1 ms    [User: 4550.6 ms, System: 965.9 ms]
  Range (min … max):   481.3 ms … 562.1 ms    10 runs

Summary
  'blkhash-async-api/build/bin/blksum -t 16 -c 25p.raw' ran
    1.13 ± 0.06 times faster than 'blkhash/build/bin/blksum -t 16 -c 25p.raw'
```

## laptop - 25% full image

```
$ hyperfine -p "sleep 3" \
            "blkhash-async-api/build/bin/blksum -t 16 -c 25p.raw" \
            "blkhash/build/bin/blksum -t 16 -c 25p.raw"
Benchmark 1: blkhash-async-api/build/bin/blksum -t 16 -c 25p.raw
  Time (mean ± σ):     631.7 ms ±  19.1 ms    [User: 4654.6 ms, System: 1305.4 ms]
  Range (min … max):   616.0 ms … 684.0 ms    10 runs

Benchmark 2: blkhash/build/bin/blksum -t 16 -c 25p.raw
  Time (mean ± σ):     734.5 ms ±   7.9 ms    [User: 4364.4 ms, System: 1579.2 ms]
  Range (min … max):   717.8 ms … 743.9 ms    10 runs

Summary
  'blkhash-async-api/build/bin/blksum -t 16 -c 25p.raw' ran
    1.16 ± 0.04 times faster than 'blkhash/build/bin/blksum -t 16 -c 25p.raw'
```

## server - 50% full image

```
$ hyperfine -p "sleep 3" \
            "blkhash-async-api/build/bin/blksum -t 16 -c 50p.raw" \
            "blkhash/build/bin/blksum -t 16 -c 50p.raw"
Benchmark 1: blkhash-async-api/build/bin/blksum -t 16 -c 50p.raw
  Time (mean ± σ):     899.8 ms ±  39.5 ms    [User: 9096.8 ms, System: 2080.3 ms]
  Range (min … max):   840.3 ms … 961.0 ms    10 runs

Benchmark 2: blkhash/build/bin/blksum -t 16 -c 50p.raw
  Time (mean ± σ):      1.073 s ±  0.037 s    [User: 9.229 s, System: 2.032 s]
  Range (min … max):    1.040 s …  1.147 s    10 runs

Summary
  'blkhash-async-api/build/bin/blksum -t 16 -c 50p.raw' ran
    1.19 ± 0.07 times faster than 'blkhash/build/bin/blksum -t 16 -c 50p.raw'
```

## laptop - 50% full image

```
$ hyperfine -p "sleep 5" \
            "blkhash-async-api/build/bin/blksum -t 16 -c 50p.raw" \
            "blkhash/build/bin/blksum -t 16 -c 50p.raw"
Benchmark 1: blkhash-async-api/build/bin/blksum -t 16 -c 50p.raw
  Time (mean ± σ):      1.234 s ±  0.060 s    [User: 9.688 s, System: 2.439 s]
  Range (min … max):    1.204 s …  1.402 s    10 runs

Benchmark 2: blkhash/build/bin/blksum -t 16 -c 50p.raw
  Time (mean ± σ):      1.457 s ±  0.043 s    [User: 9.162 s, System: 3.076 s]
  Range (min … max):    1.406 s …  1.554 s    10 runs

Summary
  'blkhash-async-api/build/bin/blksum -t 16 -c 50p.raw' ran
    1.18 ± 0.07 times faster than 'blkhash/build/bin/blksum -t 16 -c 50p.raw'
```

## server - 100% full image

```
$ hyperfine -p "sleep 8" \
            "blkhash-async-api/build/bin/blksum -t 16 -c full-6g.raw" \
            "blkhash/build/bin/blksum -t 16 -c full-6g.raw"
Benchmark 1: blkhash-async-api/build/bin/blksum -t 16 -c full-6g.raw
  Time (mean ± σ):      2.038 s ±  0.288 s    [User: 18.718 s, System: 4.642 s]
  Range (min … max):    1.735 s …  2.558 s    10 runs

Benchmark 2: blkhash/build/bin/blksum -t 16 -c full-6g.raw
  Time (mean ± σ):      2.385 s ±  0.253 s    [User: 19.323 s, System: 4.346 s]
  Range (min … max):    2.113 s …  2.898 s    10 runs

Summary
  'blkhash-async-api/build/bin/blksum -t 16 -c full-6g.raw' ran
    1.17 ± 0.21 times faster than 'blkhash/build/bin/blksum -t 16 -c full-6g.raw'
```

## laptop - 100% full image

```
$ hyperfine -p "sleep 10" \
            "blkhash-async-api/build/bin/blksum -t 16 -c full-6g.raw" \
            "blkhash/build/bin/blksum -t 16 -c full-6g.raw"
Benchmark 1: blkhash-async-api/build/bin/blksum -t 16 -c full-6g.raw
  Time (mean ± σ):      2.540 s ±  0.115 s    [User: 20.691 s, System: 4.391 s]
  Range (min … max):    2.388 s …  2.702 s    10 runs

Benchmark 2: blkhash/build/bin/blksum -t 16 -c full-6g.raw
  Time (mean ± σ):      3.048 s ±  0.212 s    [User: 21.338 s, System: 6.085 s]
  Range (min … max):    2.776 s …  3.283 s    10 runs

Summary
  'blkhash-async-api/build/bin/blksum -t 16 -c full-6g.raw' ran
    1.20 ± 0.10 times faster than 'blkhash/build/bin/blksum -t 16 -c full-6g.raw'
```
