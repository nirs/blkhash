#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

print(f"\nblkhash-bench --input-type data --digest-name {bench.DIGEST}\n")
for n in bench.threads():
    bench.blkhash("data", threads=n)
