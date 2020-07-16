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
  'print.c'
]

# dependencies
if get_option('conffile')
    inih = meson.get_compiler('c').find_library('inih')
    tio_sources += 'conffile.c'
else
    inih = dependency()
endif

tio_c_args = []

if enable_setspeed2
  tio_sources += 'setspeed2.c'
  tio_c_args += '-DHAVE_TERMIOS2'
endif

if enable_iossiospeed
  tio_sources += 'iossiospeed.c'
  tio_c_args += '-DHAVE_IOSSIOSPEED'
endif

executable('tio',
  tio_sources,
  c_args: tio_c_args,
  dependencies: inih,
  install: true )

subdir('bash-completion')
