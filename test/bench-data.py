#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import os
import bench

DIGEST = "sha256"
STREAMS = 64
TIMEOUT = 0 if "QUICK" in os.environ else 10


print(f"\nblkhash-bench --input-type data --digest-name {DIGEST}\n")

for n in bench.threads(STREAMS):
    bench.blkhash(
        "data",
        digest_name=DIGEST,
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )
