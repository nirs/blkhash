<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# blkhash performance

The `blkhash` library is up to 4 order of magnitude faster than openssl
using the same digest algorithm.

`blkhash` improves performance in several ways:

- Zero optimization - avoiding the work for computing a hash for
  unallocated ares in an image, or areas full of zeroes.
- Parallel computation - computing hashes in parallel using multiple
  threads.
- Asynchronous API - speeding up hashing with large number of threads
  and integrating well with asynchronous I/O libraries

## Zero optimization

`blkhash` can be up to 4 orders of magnitude faster than `SHA256`,
depending on the image content and number of threads.

The following graph shows zero optimization performance for 3 cases:

- data - hashing buffer full of non-zero data
- zero - hashing buffer full of zeros
- hole - hashing unallocated area

![zero optimization](../media/zero-optimization-sha256-r1m-b512k-gips.png)

## Tested hardware

The benchmarks shown here ran on *AWS c7g.metal* instance with 64 cores
running *Ubuntu*.

## How we test

`blkhash` performance is tested using the `blkhash-bench` program,
measuring the throughput of the library by hashing buffers directory
into the library, without actual I/O.

The `blkhash-bench` program is run by the `bench/run` tool:

```
bench/run bench/blkhash.yaml
```

The graphs were created using the
[plot-blkhash.py](../test/plot-blkhash.py) tool from the json test
results.

See [blksum performance](blksum-performance.md) for computing a hash of
actual disk images on storage.

See the [test/results](../test/results) directory for test results and
images.
