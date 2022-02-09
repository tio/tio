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
  'tty.c'
  ]

if tcgets2 != ''
  tio_sources += 'setspeed2.c'
endif

executable('tio',
  tio_sources,
  install: true,
)

subdir('bash-completion')
