project('cervo', 'cpp',
  version : '0.1',
  default_options : [
    'cpp_std=c++17',
    'prefix=/usr/local'
  ]
)

ncurses_dep = dependency('ncurses')

# Include directories
system_include = include_directories('/usr/include', '/usr/include/aarch64-linux-gnu')

# Optionally install shared library for PWMController
if get_option('install_libs')
  pwm_controller_lib = library('cervo',
    sources: ['src/cervo.cpp'],
    install: true,
    install_dir: '/usr/local/lib'  # System-wide install
  )
  install_headers(['src/cervo.hpp'], subdir: 'cervo')
else
  pwm_controller_lib = library('cervo',
    sources: ['src/cervo.cpp'],
    install: false
  )
endif

# Optionally install the executable
if get_option('install_exec')
  # Set default install directory to user-local ~/bin if not running with sudo
  exec_install_dir = get_option('prefix') / 'bin'
  
  # Insert the explicit check here
  if not meson.get_compiler('cpp').has_function('getuid') or run_command('id', '-u', check: true).stdout().strip() != '0'
    exec_install_dir = meson.current_source_dir() + '/bin'  # Or '$HOME/bin' for user installs
  endif

  executable('cervo',
    sources: [
      'src/main.cpp',
      'src/key_input_handler.cpp'
    ],
    include_directories: [system_include, include_directories('src')],
    dependencies: [ncurses_dep],
    link_with: pwm_controller_lib,
    install: true,
    install_dir: exec_install_dir  # User or system-level installation
  )
else
  executable('cervo',
    sources: [
      'src/main.cpp',
      'src/key_input_handler.cpp'
    ],
    include_directories: [system_include, include_directories('src')],
    dependencies: [ncurses_dep],
    link_with: pwm_controller_lib,
    install: false
  )
endif
