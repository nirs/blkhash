# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

blksum = executable(
  'blksum',
  [
    'aio-checksum.c',
    'blksum.c',
    'checksum.c',
    'file-src.c',
    'nbd-server.c',
    'nbd-src.c',
    'pipe-src.c',
    'probe.c',
    'progress.c',
    'src.c',
  ],
  include_directories : [
    common_inc,
    config_inc,
    include_inc,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    libnbd,
    dependency('threads'),
  ],
  install: true,
)
