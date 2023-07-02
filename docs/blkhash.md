<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# The blkhash library

The `blkhash` C library implements the block based hash algorithm, zero
detection, and used multiple threads to speed up the computation.

The library provides the expected interface for creating, updating,
finalizing and destroying a hash. The `blkhash_update()` function
implements zero detection, speeding up data processing. When you know
that some areas of the image are read as zeros, you can skip reading
them and use `blkhash_zero()` to add zero range to the hash.

See the [example program](example.c) for example of using the library.

See [blkhash(3)](blkhash.3.adoc) for complete documentation.

## blkhash performance

The `blkhash` library is up to 4 order of magnitude faster than openssl
using the same digest algorithm.

We measured the throughput of the `blkhash` library using `sha256`
algorithm and 64 threads for 3 operations:
- `h(non zero)` - hash non-zero data
- `h(all zeros)` - hash data that is all zeros
- `h(unallocated)` - hash an unallocated area (logically filled with zeros)

The following graph compares the throughput to `sha256` using
logarithmic scale.  The throughput of hashing unallocated area is *18,649
times higher*, hashing zeros is *76 times higher*, and hashing non-zero
data is *24 times higher*.

![blkhash vs sha256](media/blkhash-web.png)

For more info on benchmarking `blkhash` see
[test/README.md](test/#the-blkhash-bench-program) and the
[tests results](test/results).
