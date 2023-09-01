<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# blksum performance

blksum is 3-360 times faster than sha256sum, depending on the contents
of the image.

## 25% full sparse image

For typical 25% full sparse image, `blksum` is 15 times faster compared
with standard tools, using 3.7 times less cpu time:

```
$ hyperfine -w1 -p "sleep 2" "blksum 25p.raw" "sha256sum 25p.raw"
Benchmark 1: blksum 25p.raw
  Time (mean ± σ):     877.0 ms ±  33.3 ms    [User: 3380.8 ms, System: 606.0 ms]
  Range (min … max):   803.5 ms … 914.8 ms    10 runs

Benchmark 2: sha256sum 25p.raw
  Time (mean ± σ):     13.275 s ±  0.438 s    [User: 12.570 s, System: 0.672 s]
  Range (min … max):   12.828 s … 14.027 s    10 runs

Summary
  'blksum 25p.raw' ran
   15.14 ± 0.76 times faster than 'sha256sum 25p.raw'
```

## 50% full image

For images with more data the speedup is smaller, and more cpu time is
used:

```
$ hyperfine -w1 -p "sleep 2" "blksum 50p.raw" "sha256sum 50p.raw"
Benchmark 1: blksum 50p.raw
  Time (mean ± σ):      1.951 s ±  0.151 s    [User: 7.522 s, System: 1.263 s]
  Range (min … max):    1.733 s …  2.200 s    10 runs

Benchmark 2: sha256sum 50p.raw
  Time (mean ± σ):     13.716 s ±  0.428 s    [User: 13.026 s, System: 0.660 s]
  Range (min … max):   12.903 s … 14.200 s    10 runs

Summary
  'blksum 50p.raw' ran
    7.03 ± 0.59 times faster than 'sha256sum 50p.raw'
```

## 75% full image

When the image is 75% full `blkusum` is 4.5 time faster:

```
$ hyperfine -w1 -p "sleep 2" "blksum 75p.raw" "sha256sum 75p.raw"
Benchmark 1: blksum 75p.raw
  Time (mean ± σ):      3.019 s ±  0.233 s    [User: 12.063 s, System: 1.915 s]
  Range (min … max):    2.725 s …  3.518 s    10 runs

Benchmark 2: sha256sum 75p.raw
  Time (mean ± σ):     13.622 s ±  0.541 s    [User: 12.850 s, System: 0.737 s]
  Range (min … max):   13.028 s … 14.579 s    10 runs

Summary
  'blksum 75p.raw' ran
    4.51 ± 0.39 times faster than 'sha256sum 75p.raw'
```

## Reading image from a pipe

When reading from a pipe we can use only raw image data and cannot
detect image sparseness, but zero detection and multiple threads are
effective:

```
$ hyperfine -w1 -p "sleep 2" "blksum <50p.raw" "sha256sum <50p.raw"
Benchmark 1: blksum <50p.raw
  Time (mean ± σ):      1.895 s ±  0.048 s    [User: 6.426 s, System: 0.622 s]
  Range (min … max):    1.825 s …  1.986 s    10 runs

Benchmark 2: sha256sum <50p.raw
  Time (mean ± σ):     13.039 s ±  0.536 s    [User: 12.369 s, System: 0.642 s]
  Range (min … max):   12.317 s … 14.103 s    10 runs

Summary
  'blksum <50p.raw' ran
    6.88 ± 0.33 times faster than 'sha256sum <50p.raw'
```

## Empty image

The best case is a completely empty image. `blksum` detects that the
entire image is unallocated without reading any data from storage and
optimize zero hashing:

```
$ hyperfine -w1 -p "sleep 2" "blksum empty-6g.raw" "sha256sum empty-6g.raw"
Benchmark 1: blksum empty-6g.raw
  Time (mean ± σ):      37.1 ms ±   6.8 ms    [User: 11.3 ms, System: 5.1 ms]
  Range (min … max):    27.1 ms …  44.1 ms    10 runs

Benchmark 2: sha256sum empty-6g.raw
  Time (mean ± σ):     13.522 s ±  0.557 s    [User: 12.795 s, System: 0.692 s]
  Range (min … max):   12.545 s … 14.190 s    10 runs

Summary
  'blksum empty-6g.raw' ran
  364.58 ± 68.72 times faster than 'sha256sum empty-6g.raw'
```

## Image full of zeroes

A less optimal case is a fully allocated image full of zeros. `blksum`
must read the entire image, but it detects that all blocks are zeros
and optimize zero hashing:

```
$ hyperfine -w1 -p "sleep 2" "blksum zero-6g.raw" "sha256sum zero-6g.raw"
Benchmark 1: blksum zero-6g.raw
  Time (mean ± σ):      2.041 s ±  0.011 s    [User: 0.446 s, System: 1.786 s]
  Range (min … max):    2.026 s …  2.065 s    10 runs

Benchmark 2: sha256sum zero-6g.raw
  Time (mean ± σ):     13.683 s ±  0.537 s    [User: 12.854 s, System: 0.790 s]
  Range (min … max):   13.024 s … 14.661 s    10 runs

Summary
  'blksum zero-6g.raw' ran
    6.70 ± 0.27 times faster than 'sha256sum zero-6g.raw'
```

## 100% full image

The worst case is a completely full image, when nothing can be
optimized, but using multi-threading helps:

```
$ hyperfine -w1 -p "sleep 2" "blksum full-6g.raw" "sha256sum full-6g.raw"
Benchmark 1: blksum full-6g.raw
  Time (mean ± σ):      4.227 s ±  0.252 s    [User: 17.021 s, System: 2.683 s]
  Range (min … max):    3.777 s …  4.493 s    10 runs

Benchmark 2: sha256sum full-6g.raw
  Time (mean ± σ):     13.386 s ±  0.453 s    [User: 12.689 s, System: 0.655 s]
  Range (min … max):   13.006 s … 14.104 s    10 runs

Summary
  'blksum full-6g.raw' ran
    3.17 ± 0.22 times faster than 'sha256sum full-6g.raw'
```
