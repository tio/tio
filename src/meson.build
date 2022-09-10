config_h = configuration_data()
config_h.set_quoted('VERSION', meson.project_version())
config_h.set('BAUDRATE_CASES', baudrate_cases)
configure_file(output: 'config.h', configuration: config_h)

tio_sources = [
  'error.c',
  'log.c',
  'main.c',
  'options.c',
  'misc.c',
  'tty.c',
  'print.c',
  'configfile.c',
  'signals.c',
  'socket.c',
  'setspeed.c',
  'rs485.c',
  'timestamp.c',
  'alert.c'
]

tio_dep = dependency('inih', required: true,
                     fallback : ['libinih', 'inih_dep'],
                     default_options: ['default_library=static', 'distro_install=false'])

tio_c_args = ['-Wno-unused-result']

if enable_setspeed2
  tio_c_args += '-DHAVE_TERMIOS2'
endif

if enable_iossiospeed
  tio_c_args += '-DHAVE_IOSSIOSPEED'
endif

if enable_rs485
  tio_c_args += '-DHAVE_RS485'
endif

executable('tio',
  tio_sources,
  c_args: tio_c_args,
  dependencies: tio_dep,
  install: true )

subdir('bash-completion')
