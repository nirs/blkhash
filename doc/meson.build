# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

a2x = find_program('a2x')

blksum_1 = custom_target(
  'blksum.1',
  command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@/doc', '@INPUT@'],
  input: 'blksum.1.adoc',
  output: 'blksum.1',
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man1')
)

blkhash_3 = custom_target(
  'blkhash.3',
  command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@/doc', '@INPUT@'],
  input: 'blkhash.3.adoc',
  output: [
    'blkhash_new.3',
    'blkhash_update.3',
    'blkhash_zero.3',
    'blkhash_final.3',
    'blkhash_free.3',
  ],
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man3')
)

# Make sure the example compiles and run.
example = executable(
  'example',
  'example.c',
  include_directories : include,
  link_with: blkhash_lib,
  dependencies: [
    openssl,
  ],
)
test('example', example)
