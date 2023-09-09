# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

project(
  'blkhash', 'c',
  version: '0.9.1',
  license: 'LGPL-2.1-or-later',
  meson_version: '>= 0.58.0',
  default_options: [
    'b_pie=true',
    'buildtype=debugoptimized',
    'c_std=gnu99',
    'warning_level=2',
  ],
)

conf_data = configuration_data()

libnbd = dependency(
  'libnbd',
  version : '>=1.2.2',  # RHEL 8.5
  required: get_option('nbd'),
)
conf_data.set('HAVE_NBD', libnbd.found())

openssl = dependency('openssl')

compiler = meson.get_compiler('c')

detect_cache_line_size = '''
#include <stdio.h>

#include <sys/param.h>
#if defined(linux)
  #include <unistd.h>
#endif
#if defined(__APPLE__)
  #include <sys/types.h>
  #include <sys/sysctl.h>
#endif

int main()
{
  size_t cache_line_size = 64;
#ifdef CACHE_LINE_SIZE
  cache_line_size = CACHE_LINE_SIZE;
#endif
#ifdef _SC_LEVEL1_DCACHE_LINESIZE
  cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#endif
#if defined(__APPLE__)
  size_t size = sizeof(cache_line_size);
  sysctlbyname("hw.cachelinesize", &cache_line_size, &size, 0, 0);
#endif
  printf("%ld\n", cache_line_size);
}
'''

result = compiler.run(
  detect_cache_line_size,
  name: 'detect cache line size',
)
if result.returncode() == 0
  conf_data.set('CACHE_LINE_SIZE', result.stdout().strip())
endif

summary_info = {}
summary_info += {'nbd': conf_data.get('HAVE_NBD')}
summary_info += {'cache line size': conf_data.get('CACHE_LINE_SIZE')}
summary(summary_info, bool_yn: true, section: 'config')

configure_file(
  output : 'blkhash-config.h',
  configuration : conf_data,
)

config_inc = include_directories('.')

subdir('include')
subdir('common')
subdir('lib')
subdir('bin')
subdir('test')
subdir('man')

pkg_config = import('pkgconfig')
pkg_config.generate(blkhash_lib)
