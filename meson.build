project('tio', 'c',
    version : '3.5',
    license : [ 'GPL-2'],
    meson_version : '>= 0.53.2',
    default_options : [ 'warning_level=2', 'buildtype=release', 'c_std=gnu99' ]
)

# The tag date of the project_version(), update when the version bumps.
version_date = '2024-06-15'

# Test for dynamic baudrate configuration interface
compiler = meson.get_compiler('c')
enable_setspeed2 = false
enable_iossiospeed = false
if host_machine.system() != 'darwin'
  if compiler.check_header('asm-generic/ioctls.h')
    enable_setspeed2 = compiler.has_header_symbol('asm-generic/ioctls.h', 'TCGETS2')
  endif
else
  if compiler.check_header('IOKit/serial/ioss.h')
    enable_iossiospeed = compiler.has_header_symbol('IOKit/serial/ioss.h', 'IOSSIOSPEED')
  endif
endif

# Test for supported baudrates
test_baudrates = [
    0,
    50,
    75,
    110,
    134,
    150,
    200,
    300,
    600,
    1200,
    1800,
    2400,
    4800,
    7200,
    9600,
    14400,
    19200,
    28800,
    38400,
    57600,
    76800,
    115200,
    230400,
    460800,
    500000,
    576000,
    921600,
    1000000,
    1152000,
    1500000,
    2000000,
    2500000,
    3000000,
    3500000,
    4000000 ]

baudrates = ''
baudrate_cases = ''
foreach rate : test_baudrates
  baudrate = rate.to_string()
  value = compiler.get_define('B' + baudrate, prefix: '#include <termios.h>')
  if value != ''
    baudrates = baudrates + baudrate + ' '
    baudrate_cases = baudrate_cases + ' case ' + baudrate + ': baudrate = B' + baudrate + '; break;'
  endif
endforeach

# Test for RS-485 support on Linux
enable_rs485 = false
if host_machine.system() == 'linux'
  if compiler.check_header('linux/serial.h')
    enable_rs485 = compiler.has_header_symbol('sys/ioctl.h', 'TIOCSRS485')
  endif
endif

subdir('src')

install_man_pages = get_option('install_man_pages')
if install_man_pages
  subdir('man')
endif
