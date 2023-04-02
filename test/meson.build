# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

unity_dep = dependency('unity', fallback : ['unity', 'unity_dep'])

blkhash_test = executable(
  'blkhash-test',
  'blkhash-test.c',
  include_directories : [
    config_inc,
    include,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    openssl,
    unity_dep,
  ],
)

zero_bench = executable(
  'zero-bench',
  'zero-bench.c',
  include_directories : [
    config_inc,
    include,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    openssl,
    unity_dep,
  ],
)

cc = meson.get_compiler('c')
rt_dep = cc.find_library('rt')

blkhash_bench = executable(
  'blkhash-bench',
  'blkhash-bench.c',
  include_directories : include,
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    openssl,
    # Works without rt_dep, but better to make this excplicit.
    rt_dep,
  ],
)

openssl_bench = executable(
  'openssl-bench',
  'openssl-bench.c',
  include_directories : include,
  link_with: [
    common_lib,
  ],
  dependencies: [
    openssl,
  ],
)

env = environment()
env.set('BUILD_DIR', meson.current_build_dir())
env.set('QUICK', '1')
env.set('BLKSUM', blksum.full_path())
env.set('HAVE_NBD', libnbd.found() ? '1' : '')

test('blkhash-test', blkhash_test)
test('zero-bench', zero_bench, args: ['quick'])

test(
  'blkhash-bench',
  find_program('blkhash-bench.py'),
  depends: blkhash_bench,
  env: env,
  workdir: meson.current_source_dir(),
)

# On Centos Stream 8 pytest is installed as pytest-3
pytest = find_program('pytest', '/usr/bin/pytest-3', required: false)

if pytest.found()
  test(
    'bench-test',
    pytest,
    args: ['bench_test.py'],
    depends: [
      blkhash_bench,
      openssl_bench,
    ],
    env: env,
    workdir: meson.current_source_dir(),
  )

  test(
    'blksum-test',
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
