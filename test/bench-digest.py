#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

print(f"\ndigest-bench --digest-name {args.digest_name}\n")
for n in bench.threads(args.max_threads):
    bench.digest(
        digest_name=args.digest_name,
        threads=n,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
