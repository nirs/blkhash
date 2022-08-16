# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

project(
  'blkhash', 'c',
  version: '0.3.0',
  license: 'LGPL-2.1-or-later',
  meson_version: '>= 0.54.0',
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

openssl = dependency('openssl')

blkhash_lib = library(
  'blkhash',
  [
    'blkhash.c',
    'worker.c',
    'block.c',
    'config.c',
    'zero.c',
  ],
  version: meson.project_version(),
  soversion: '0',
  dependencies: [
    openssl,
    dependency('threads'),
  ],
  install: true,
)

install_headers('blkhash.h')

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
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    libnbd,
    openssl,
    dependency('threads'),
  ],
  install: true,
)

a2x = find_program('a2x')

blksum_1 = custom_target(
  'blksum.1',
  command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@', '@INPUT@'],
  input: 'blksum.1.adoc',
  output: 'blksum.1',
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man1')
)

blksum_1 = custom_target(
  'blkhash.3',
  command: [a2x, '--format=manpage', '--destination-dir=@BUILD_ROOT@', '@INPUT@'],
  input: 'blkhash.3.adoc',
  output: [
    'blkhash_new.3',
    'blkhash_update.3',
    'blkhash_zero.3',
    'blkhash_final.3',
    'blkhash_reset.3',
    'blkhash_free.3',
  ],
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man3')
)

example = executable(
  'example',
  [
    'example.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    openssl,
  ],
)

unity_dep = dependency('unity', fallback : ['unity', 'unity_dep'])

worker_test = executable(
  'worker_test',
  [
    'worker_test.c',
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    openssl,
    unity_dep,
  ],
)

blkhash_test = executable(
  'blkhash_test',
  [
    'blkhash_test.c',
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: unity_dep,
)

zero_bench = executable(
  'zero_bench',
  [
    'zero_bench.c',
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    openssl,
    unity_dep,
  ],
)

blkhash_bench = executable(
  'blkhash_bench',
  [
    'blkhash_bench.c',
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    openssl,
    unity_dep,
  ],
)

test('worker-test', worker_test)
test('blkhash-test', blkhash_test)
test('zero-bench', zero_bench, args: ['quick'])
test('blkhash-bench', blkhash_bench, args: ['quick'])
test('example', example)

# On Centos Stream 8 pytest is installed as pytest-3
pytest = find_program('pytest', '/usr/bin/pytest-3', required: false)

if pytest.found()
  env = environment()
  env.set('BLKSUM', blksum.full_path())
  env.set('HAVE_NBD', libnbd.found() ? '1' : '')
  test(
    'blksum',
    pytest,
    args: ['blksum_test.py'],
    depends: blksum,
    timeout: 120,
    env: env,
    workdir: meson.current_source_dir(),
  )
endif

# `resuse lint` works only in git repo.
git = run_command('test', '-d', '.git', check: false).returncode() == 0
if git
  reuse = find_program('reuse', required: false)
  if reuse.found()
    test('reuse', reuse, args: ['lint'])
  endif
endif
