# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

unity_dep = dependency('unity', fallback : ['unity', 'unity_dep'])

blkhash_test = executable(
  'blkhash_test',
  'blkhash_test.c',
  include_directories : include,
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
  'zero_bench',
  'zero_bench.c',
  include_directories : include,
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    openssl,
    unity_dep,
  ],
)

blkhash_bench = executable(
  'blkhash_bench',
  'blkhash_bench.c',
  include_directories : include,
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    openssl,
    unity_dep,
  ],
)

test('blkhash-test', blkhash_test)
test('zero-bench', zero_bench, args: ['quick'])
test('blkhash-bench', blkhash_bench, args: ['quick'])

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
