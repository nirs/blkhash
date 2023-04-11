# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

common_inc = include_directories('.')

common_lib = static_library(
  'common',
  'util.c',
  include_directories: [
    include_inc,
  ],
  dependencies: [
    openssl,
  ],
)
