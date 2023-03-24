#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

STREAMS = 64

print("\nblkhash-bench --digest-name sha256 --input-type data\n")

bench.blkhash("data", "0.6g", digest_name="sha256", threads=1, streams=STREAMS)
bench.blkhash("data", "1.0g", digest_name="sha256", threads=2, streams=STREAMS)
bench.blkhash("data", "2.0g", digest_name="sha256", threads=4, streams=STREAMS)
bench.blkhash("data", "2.2g", digest_name="sha256", threads=8, streams=STREAMS)
bench.blkhash("data", "3.0g", digest_name="sha256", threads=16, streams=STREAMS)
bench.blkhash("data", "2.9g", digest_name="sha256", threads=32, streams=STREAMS)
bench.blkhash("data", "2.9g", digest_name="sha256", threads=64, streams=STREAMS)

print("\nblkhash-bench --digest-name sha256 --input-type zero\n")

bench.blkhash("zero", "50g", digest_name="sha256", threads=1, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="sha256", threads=2, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="sha256", threads=4, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="sha256", threads=8, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="sha256", threads=16, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="sha256", threads=32, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="sha256", threads=64, streams=STREAMS)

print("\nblkhash-bench --digest-name sha256 --input-type hole\n")

bench.blkhash("hole", "960g", digest_name="sha256", threads=1, streams=STREAMS)
bench.blkhash("hole", "1.8t", digest_name="sha256", threads=2, streams=STREAMS)
bench.blkhash("hole", "3.5t", digest_name="sha256", threads=4, streams=STREAMS)
bench.blkhash("hole", "4.0t", digest_name="sha256", threads=8, streams=STREAMS)
bench.blkhash("hole", "4.7t", digest_name="sha256", threads=16, streams=STREAMS)
bench.blkhash("hole", "4.6t", digest_name="sha256", threads=32, streams=STREAMS)
bench.blkhash("hole", "4.6t", digest_name="sha256", threads=64, streams=STREAMS)

print("\nblkhash-bench --digest-name null --input-type data\n")

bench.blkhash("data", "21g", digest_name="null", threads=1, streams=STREAMS)
bench.blkhash("data", "21g", digest_name="null", threads=2, streams=STREAMS)
bench.blkhash("data", "21g", digest_name="null", threads=4, streams=STREAMS)
bench.blkhash("data", "21g", digest_name="null", threads=8, streams=STREAMS)
bench.blkhash("data", "21g", digest_name="null", threads=16, streams=STREAMS)
bench.blkhash("data", "21g", digest_name="null", threads=32, streams=STREAMS)
bench.blkhash("data", "21g", digest_name="null", threads=64, streams=STREAMS)

print("\nblkhash-bench --digest-name null --input-type zero\n")

bench.blkhash("zero", "50g", digest_name="null", threads=1, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="null", threads=2, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="null", threads=4, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="null", threads=8, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="null", threads=16, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="null", threads=32, streams=STREAMS)
bench.blkhash("zero", "50g", digest_name="null", threads=64, streams=STREAMS)

print("\nblkhash-bench --digest-name null --input-type hole\n")

bench.blkhash("hole", "31t", digest_name="null", threads=1, streams=STREAMS)
bench.blkhash("hole", "61t", digest_name="null", threads=2, streams=STREAMS)
bench.blkhash("hole", "118t", digest_name="null", threads=4, streams=STREAMS)
bench.blkhash("hole", "157t", digest_name="null", threads=8, streams=STREAMS)
bench.blkhash("hole", "211t", digest_name="null", threads=16, streams=STREAMS)
bench.blkhash("hole", "190t", digest_name="null", threads=32, streams=STREAMS)
bench.blkhash("hole", "190t", digest_name="null", threads=64, streams=STREAMS)
