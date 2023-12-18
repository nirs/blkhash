# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

unity_dep = dependency('unity', fallback : ['unity', 'unity_dep'])

blkhash_test = executable(
  'blkhash-test',
  'blkhash-test.c',
  include_directories : [
    blkhash_inc,
    common_inc,
    config_inc,
    include_inc,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    unity_dep,
  ],
)

digest_test = executable(
  'digest-test',
  'digest-test.c',
  include_directories : [
    blkhash_inc,
    common_inc,
    config_inc,
    include_inc,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    unity_dep,
  ],
)

util_test = executable(
  'util-test',
  'util-test.c',
  include_directories : [
    common_inc,
  ],
  link_with: [
    common_lib,
  ],
  dependencies: [
    unity_dep,
  ],
)

zero_bench = executable(
  'zero-bench',
  'zero-bench.c',
  include_directories: [
    blkhash_inc,
    common_inc,
    config_inc,
    include_inc,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    unity_dep,
  ],
)

blkhash_bench = executable(
  'blkhash-bench',
  'blkhash-bench.c',
  'benchmark.c',
  include_directories: [
    common_inc,
    config_inc,
    include_inc,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
)

digest_bench = executable(
  'digest-bench',
  'digest-bench.c',
  'benchmark.c',
  include_directories: [
    blkhash_inc,
    common_inc,
    config_inc,
    include_inc,
  ],
  link_with: [
    blkhash_lib,
    common_lib,
  ],
  dependencies: [
    dependency('threads'),
  ],
)

env = environment()
env.set('BUILD_DIR', meson.current_build_dir())
env.set('BLKSUM', blksum.full_path())
env.set('HAVE_NBD', libnbd.found() ? '1' : '')

test('blkhash-test', blkhash_test)
test('digest-test', digest_test)
test('util-test', util_test)
test('zero-bench', zero_bench, args: ['quick'])

benchmarks = [
  'bench-blkhash-data',
  'bench-blkhash-zero',
  'bench-blkhash-hole',
  'bench-blkhash-null',
  'bench-digest',
]

foreach b : benchmarks
  test(
    b,
    find_program(b + '.py'),
    args: ['--timeout=0', '--cool-down=0'],
    depends: blkhash_bench,
    env: env,
    workdir: meson.current_source_dir(),
  )
endforeach

# On Centos Stream 8 pytest is installed as pytest-3
pytest = find_program('pytest', '/usr/bin/pytest-3', required: false)

if pytest.found()
  test(
    'bench-test',
    pytest,
    args: ['bench_test.py'],
    depends: [
      blkhash_bench,
      digest_bench,
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
