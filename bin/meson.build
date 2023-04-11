# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

blksum = executable(
  'blksum',
  [
    'aio-checksum.c',
    'blksum.c',
    'checksum.c',
    'file.c',
    'list-digests.c',
    'nbd.c',
    'nbdserver.c',
    'pipe.c',
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
    openssl,
    dependency('threads'),
  ],
  install: true,
)
