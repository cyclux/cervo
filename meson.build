project('cervo', 'cpp',
  version : '0.1',
  default_options : [
    'cpp_std=c++17',
    'prefix=~/projects/cervo'
  ]
)

ncurses_dep = dependency('ncurses')

# Define the path to the kernel headers
kernel_headers_include = include_directories('/usr/src/linux-headers-6.6.31+rpt-common-rpi/include')

executable('cervo',
  sources: ['src/main.cpp'],
  include_directories: kernel_headers_include,
  dependencies: [ncurses_dep],
  install: true,
  install_dir: 'bin'
)
