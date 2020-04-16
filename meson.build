project('flathash', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_FLATHASH']

shlib = shared_library('flathash', 'flathash.cpp',
  install : true,
  cpp_args : lib_args,
  gnu_symbol_visibility : 'hidden',
)

test_exe = executable('flathash', 'flathash_test.cpp',
  link_with : shlib)
test('flathash', test_exe)

# Make this library usable as a Meson subproject.
flathash_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : shlib)

# Make this library usable from the system's
# package manager.
install_headers('flathash.hpp', subdir : 'flathash')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name : 'flathash',
  filebase : 'flathash',
  description : 'Meson sample project.',
  subdirs : 'flathash',
  libraries : shlib,
  version : '0.1',
)
