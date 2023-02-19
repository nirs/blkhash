# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

blksum = executable(
  'blksum',
  [
    'blksum.c',
    'list-digests.c',
    'file.c',
    'nbd.c',
    'nbdserver.c',
    'parallel.c',
    'pipe.c',
    'probe.c',
    'progress.c',
    'simple.c',
    'src.c',
  ],
  include_directories : include,
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    libnbd,
    openssl,
    dependency('threads'),
  ],
  install: true,
)
