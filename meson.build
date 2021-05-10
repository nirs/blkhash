project('blkhash', 'c', version: '0.1')

blkhash_lib = library(
  'blkhash',
  [
    'blkhash.c',
  ],
  version: meson.project_version(),
  soversion: '0',
  dependencies: dependency('libcrypto'),
  install: true,
)

install_headers('blkhash.h')

blksum = executable(
  'blksum',
  [
    'blksum.c',
    'file.c',
    'nbd.c',
    'pipe.c',
    'src.c',
    'simple.c',
    'parallel.c',
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    dependency('libnbd'),
    dependency('libcrypto'),
    dependency('threads'),
  ],
  install: true,
)

unity_dir = include_directories('unity/src')

unity_lib = static_library(
  'unity',
  files('unity/src/unity.c'),
  include_directories: unity_dir
)

blkhash_test = executable(
  'blkhash_test',
  [
    'blkhash_test.c',
    'util.c',
  ],
  include_directories : unity_dir,
  link_with: [blkhash_lib, unity_lib]
)

test('blkhash', blkhash_test)

pytest = find_program('pytest', required: false)

if pytest.found()
  test(
    'blksum', pytest,
    depends: blksum,
    env: ['BLKSUM=' + blksum.full_path()],
    workdir: meson.current_source_dir(),
  )
endif
