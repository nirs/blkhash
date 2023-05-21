#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

for input_type in "data", "zero", "hole":
    print(f"\nblkhash-bench --digest-name null --input-type {input_type}\n")
    for n in bench.threads():
        bench.blkhash(
            input_type,
            digest_name="null",
            threads=n,
            timeout_seconds=args.timeout,
            cool_down=args.cool_down,
        )

    if input_type != "hole":
        print(f"\nblkhash-bench --digest-name null --input-type {input_type} --aio\n")
        for n in bench.threads():
            bench.blkhash(
                input_type,
                digest_name="null",
                threads=n,
                aio=True,
                timeout_seconds=args.timeout,
                cool_down=args.cool_down,
            )
