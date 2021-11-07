project('blkhash', 'c', version: '0.1')

libnbd = dependency('libnbd', required: get_option('nbd'))
if libnbd.found()
  add_project_arguments('-DHAVE_NBD=1', language : 'c')
endif

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
    'parallel.c',
    'pipe.c',
    'probe.c',
    'simple.c',
    'src.c',
    'util.c',
  ],
  link_with: blkhash_lib,
  dependencies: [
    libnbd,
    dependency('libcrypto'),
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
    dependency('libcrypto'),
  ],
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
test('example', example)

pytest = find_program('pytest', required: false)

if pytest.found()
  env = environment()
  env.set('BLKSUM', blksum.full_path())
  env.set('HAVE_NBD', libnbd.found() ? '1' : '')
  test(
    'blksum',
    pytest,
    depends: blksum,
    env: env,
    workdir: meson.current_source_dir(),
  )
endif
