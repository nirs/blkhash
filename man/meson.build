# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

# TODO: use allowed() when dropping Centos 8 (requires meson 0.59.0).
if not get_option('man').disabled()

  a2x = find_program('a2x')

  blksum_1 = custom_target(
    'blksum.1',
    command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@/man', '@INPUT@'],
    input: 'blksum.1.adoc',
    output: 'blksum.1',
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man1')
  )

  blkhash_3 = custom_target(
    'blkhash.3',
    command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@/man', '@INPUT@'],
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

  blkhash_aio_3 = custom_target(
    'blkhash-aio.3',
    command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@/man', '@INPUT@'],
    input: 'blkhash-aio.3.adoc',
    output: [
      'blkhash_aio_update.3',
      'blkhash_aio_completion_fd.3',
      'blkhash_aio_completions.3',
    ],
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man3')
  )

  blkhash_opt_3 = custom_target(
    'blkhash-opts.3',
    command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@/man', '@INPUT@'],
    input: 'blkhash-opts.3.adoc',
    output: [
      'blkhash_opts_new.3',
      'blkhash_opts_free.3',
      'blkhash_opts_set_block_size.3',
      'blkhash_opts_set_threads.3',
      'blkhash_opts_set_queue_depth.3',
    ],
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man3')
  )

  # Make sure the examples compile and run.

  example = executable(
    'example',
    'example.c',
    include_directories: [
      include_inc,
    ],
    link_with: blkhash_lib,
  )

  example_aio = executable(
    'example-aio',
    'example-aio.c',
    include_directories : [
      include_inc,
    ],
    link_with: blkhash_lib,
  )

  test('example', example)
  test('example-aio', example_aio)

endif
