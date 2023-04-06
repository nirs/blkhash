# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

blkhash_lib = library(
  'blkhash',
  [
    'blkhash.c',
    'config.c',
    'stream.c',
    'submission.c',
    'worker.c',
    'zero.c',
  ],
  include_directories : [
    include,
    config_inc,
  ],
  version: meson.project_version(),
  soversion: '0',
  link_with: [
    common_lib,
  ],
  dependencies: [
    openssl,
    dependency('threads'),
  ],
  install: true,
)
