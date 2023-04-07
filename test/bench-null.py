#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import os
import bench

DIGEST = "null"
STREAMS = 64
TIMEOUT = 0 if "QUICK" in os.environ else 10


for input_type in "data", "zero", "hole":
    print(f"\nblkhash-bench --digest-name {DIGEST} --input-type {input_type}\n")
    for n in bench.threads(STREAMS):
        bench.blkhash(
            input_type,
            digest_name=DIGEST,
            timeout_seconds=TIMEOUT,
            threads=n,
            streams=STREAMS,
        )
