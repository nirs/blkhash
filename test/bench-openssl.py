#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

print(f"\nopenssl-bench --digest-name {bench.DIGEST}\n")
for n in bench.threads():
    bench.openssl(
        threads=n,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
