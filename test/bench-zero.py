#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type zero\n")
for n in bench.threads():
    bench.blkhash("zero", threads=n)

print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type zero --aio\n")
for n in bench.threads():
    bench.blkhash("zero", threads=n, aio=True)
