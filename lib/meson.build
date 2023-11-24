# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

blkhash_inc = include_directories('.')

blkhash_lib = library(
  'blkhash',
  [
    'blkhash.c',
    'completion.c',
    'config.c',
    'digest.c',
    'event.c',
    'stream.c',
    'submission.c',
    'worker.c',
    'zero.c',
  ],
  include_directories : [
    blkhash_inc,
    common_inc,
    config_inc,
    include_inc,
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
