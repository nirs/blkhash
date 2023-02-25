# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

project(
  'blkhash', 'c',
  version: '0.8.0',
  license: 'LGPL-2.1-or-later',
  meson_version: '>= 0.58.0',
  default_options: [
    'b_pie=true',
    'buildtype=debugoptimized',
    'c_std=gnu99',
    'warning_level=2',
  ],
)

libnbd = dependency(
  'libnbd',
  version : '>=1.2.2',  # RHEL 8.5
  required: get_option('nbd'),
)
if libnbd.found()
  add_project_arguments('-DHAVE_NBD=1', language : 'c')
endif

include = include_directories('include')
openssl = dependency('openssl')

subdir('include')
subdir('lib')
subdir('common')
subdir('bin')
subdir('test')
subdir('doc')

pkg_config = import('pkgconfig')
pkg_config.generate(blkhash_lib)
