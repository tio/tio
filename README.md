
[![tio](images/tio-icon.png)]()

# tio - a serial device I/O tool

[![](https://img.shields.io/circleci/build/github/tio/tio)](https://circleci.com/github/tio/tio/tree/master)
[![](https://img.shields.io/github/v/release/tio/tio?sort=semver)](https://github.com/tio/tio/releases)
[![](https://img.shields.io/repology/repositories/tio)](https://repology.org/project/tio/versions)
<!-- [![](https://img.shields.io/tokei/lines/github/tio/tio)](https://github.com/tio/tio) -->

## 1. Introduction

tio is a serial device tool which features a straightforward command-line and
configuration file interface to easily connect to serial TTY devices for basic
I/O operations.

<p align="center">
<img src="images/tio-demo.gif">
</p>

### 1.1 Motivation

To make a simpler serial device tool for talking with serial TTY devices with
less focus on classic terminal/modem features and more focus on the needs of
embedded developers and hackers.

tio was originally created as an alternative to
[screen](https://www.gnu.org/software/screen) for connecting to serial devices
when used in combination with [tmux](https://tmux.github.io).

## 2. Features

 * Easily connect to serial TTY devices
 * Automatic connect and reconnect
 * Sensible defaults (115200 8n1)
 * Support for non-standard baud rates
 * Support for mark and space parity
 * X-modem (1K/CRC) and Y-modem file upload
 * Support for RS-485 mode
 * List available serial devices by ID
 * Show RX/TX statistics
 * Toggle serial lines
 * Pulse serial lines with configurable pulse duration
 * Local echo support
 * Remapping of characters (nl, cr-nl, bs, lowercase to uppercase, etc.)
 * Line timestamps
 * Support for delayed output per character
 * Support for delayed output per line
 * Switchable independent input and output mode (normal vs hex)
 * Log to file with automatic or manual naming of log file
 * Configuration file support
 * Activate sub-configurations by name or pattern
 * Redirect I/O to UNIX socket or IPv4/v6 network socket for scripting or TTY sharing
 * Pipe input and/or output
 * Bash completion on options, serial device names, and sub-configuration names
 * Configurable text color
 * Visual or audible alert on connect/disconnect
 * Remapping of prefix key
 * Support NO_COLOR env variable as per no-color.org
 * Man page documentation
 * Lua scripting support for automation
   * Run script manually or automatically at connect once/always/never
   * Simple expect/send like functionality with support for regular expressions
   * Manipulate port control lines (useful for microcontroller reset/boot etc.)
   * Send files via x/y-modem protocol
 * Plays nicely with [tmux](https://tmux.github.io)

## 3. Usage

For more usage details please see the man page documentation
[here](https://raw.githubusercontent.com/tio/tio/master/man/tio.1.txt).

### 3.1 Command-line

The command-line interface is straightforward as reflected in the output from
'tio --help':
```
Usage: tio [<options>] <tty-device|sub-config>

Connect to TTY device directly or via sub-configuration.

Options:
  -b, --baudrate <bps>                   Baud rate (default: 115200)
  -d, --databits 5|6|7|8                 Data bits (default: 8)
  -f, --flow hard|soft|none              Flow control (default: none)
  -s, --stopbits 1|2                     Stop bits (default: 1)
  -p, --parity odd|even|none|mark|space  Parity (default: none)
  -o, --output-delay <ms>                Output character delay (default: 0)
  -O, --output-line-delay <ms>           Output line delay (default: 0)
      --line-pulse-duration <duration>   Set line pulse duration
  -n, --no-autoconnect                   Disable automatic connect
  -e, --local-echo                       Enable local echo
      --input-mode normal|hex|line       Select input mode (default: normal)
      --output-mode normal|hex           Select output mode (default: normal)
  -t, --timestamp                        Enable line timestamp
      --timestamp-format <format>        Set timestamp format (default: 24hour)
  -L, --list-devices                     List available serial devices by ID
  -l, --log                              Enable log to file
      --log-file <filename>              Set log filename
      --log-directory <path>             Set log directory path for automatic named logs
      --log-append                       Append to log file
      --log-strip                        Strip control characters and escape sequences
  -m, --map <flags>                      Map characters
  -c, --color 0..255|bold|none|list      Colorize tio text (default: bold)
  -S, --socket <socket>                  Redirect I/O to socket
      --rs-485                           Enable RS-485 mode
      --rs-485-config <config>           Set RS-485 configuration
      --alert bell|blink|none            Alert on connect/disconnect (default: none)
      --mute                             Mute tio
      --script <string>                  Run script from string
      --script-file <filename>           Run script from file
      --script-run once|always|never     Run script on connect (default: always)
  -v, --version                          Display version
  -h, --help                             Display help

Options and sub-configurations may be set via configuration file.

See the man page for more details.
```

By default tio automatically connects to the provided TTY device if present.
If the device is not present, it will wait for it to appear and then connect.
If the connection is lost (eg. device is unplugged), it will wait for the
device to reappear and then reconnect. However, if the `--no-autoconnect`
option is provided, tio will exit if the device is not present or an
established connection is lost.

tio features full bash autocompletion.

#### 3.1.1 Examples

Typical use is without options:
```
$ tio /dev/ttyUSB0
```

Which corresponds to the commonly used default options:
```
$ tio --baudrate 115200 --databits 8 --flow none --stopbits 1 --parity none /dev/ttyUSB0
```

It is recommended to connect serial TTY devices by ID:
```
$ tio /dev/serial/by-id/usb-FTDI_TTL232R-3V3_FTGQVXBL-if00-port0
```
Using serial devices by ID ensures that tio automatically reconnects to the
correct serial device if it is disconnected and then reconnected.

List available serial devices by ID:
```
$ tio --list-devices
```
Note: One can also use tio shell completion on /dev which will automatically
list all available serial TTY devices.

Log to file with autogenerated filename:
```
$ tio --log /dev/ttyUSB0
```

Enable ISO8601 timestamps per line:
```
$ tio --timestamp --timestamp-format iso8601 /dev/ttyUSB0
```

Redirect I/O to IPv4 network socket on port 4242:
```
$ tio --socket inet:4242 /dev/ttyUSB0
```

Pipe data to the serial device:
```
$ cat data.bin | tio /dev/ttyUSB0
```

Pipe command to serial device and wait for line response within 1 second:
```
$ echo "*IDN?" | tio /dev/ttyACM0 --script "expect('\r\n', 1000)" --mute
KORAD KD3305P V4.2 SN:32475045
```

### 3.2 Key commands

Various in session key commands are supported. When tio is started, press
ctrl-t ? to list the available key commands.

```
[15:02:53.269] Key commands:
[15:02:53.269]  ctrl-t ?       List available key commands
[15:02:53.269]  ctrl-t b       Send break
[15:02:53.269]  ctrl-t c       Show configuration
[15:02:53.269]  ctrl-t e       Toggle local echo mode
[15:02:53.269]  ctrl-t f       Toggle log to file
[15:02:53.269]  ctrl-t F       Flush data I/O buffers
[15:02:53.269]  ctrl-t g       Toggle serial port line
[15:02:53.269]  ctrl-t i       Toggle input mode
[15:02:53.269]  ctrl-t l       Clear screen
[15:02:53.269]  ctrl-t L       Show line states
[15:02:53.269]  ctrl-t m       Toggle MSB to LSB bit order
[15:02:53.269]  ctrl-t o       Toggle output mode
[15:02:53.269]  ctrl-t p       Pulse serial port line
[15:02:53.269]  ctrl-t q       Quit
[15:02:53.269]  ctrl-t r       Run script
[15:02:53.269]  ctrl-t s       Show statistics
[15:02:53.269]  ctrl-t t       Toggle line timestamp mode
[15:02:53.269]  ctrl-t U       Toggle conversion to uppercase on output
[15:02:53.269]  ctrl-t v       Show version
[15:02:53.269]  ctrl-t x       Send file via Xmodem
[15:02:53.269]  ctrl-t y       Send file via Ymodem
[15:02:53.269]  ctrl-t ctrl-t  Send ctrl-t character
```

If needed, the prefix key (ctrl-t) can be remapped via configuration file.

### 3.3 Lua script API

Tio suppots Lua scripting to easily automate interaction with the tty device.

In addition to the Lua API tio makes the following functions available:

```
  expect(string, timeout)
        Expect string - waits for string to match or timeout before continueing.

        Supports regular expressions. Special characters must be escaped with '\\'.

        Timeout is in milliseconds, defaults to 0 meaning it will wait forever.

  send(string)
        Send string.

  modem_send(file, protocol)
        Send file using x/y-modem protocol.

        Protocol can be any of XMODEM_1K, XMODEM_CRC, YMODEM.

  exit(code)
        Exit with code.

  high(line)
        Set tty line high.

  low(line)
        Set tty line low.

  toggle(line)
        Toggle the tty line.

  sleep(seconds)
        Sleep for seconds.

  msleep(ms)
        Sleep for milliseconds.

  config_high(line)
        Set tty line state configuration to high.

  config_low(line)
        Set tty line state configuration to low.

  apply_config()
        Apply tty line state configuration.

        Using the line state configuration API instead of high()/low() will
        help to make the lines physically switch as simultaneously as possible.
        This may solve timing issues on some platforms.

  Note: Line can be any of DTR, RTS, CTS, DSR, CD, RI
```

### 3.4 Configuration file

Options can be set via the configuration file first found in any of the
following locations in the order listed:
 - $XDG_CONFIG_HOME/tio/config
 - $HOME/.config/tio/config
 - $HOME/.tioconfig

The configuration file supports sub-configurations using named sections which can
be activated via the command-line by name or pattern. A sub-configuration
specifies which TTY device to connect to and other options.

### 3.4.1 Examples

Example configuration file:

```
# Defaults
baudrate = 9600
databits = 8
parity = none
stopbits = 1
color = 10

[rpi3]
device = /dev/serial/by-id/usb-FTDI_TTL232R-3V3_FTGQVXBL-if00-port0
baudrate = 115200
no-autoconnect = enable
log = enable
log-file = rpi3.log
line-pulse-duration = DTR=200,RTS=150
color = 11

[svf2]
device = /dev/ttyUSB0
script = expect("login: "); send("root\n"); expect("Password: "); send("root\n")
color = 12

[esp32]
device = /dev/serial/by-id/usb-0403_6014-if00-port0
script = high(DTR); low(RTS); msleep(100); low(DTR); high(RTS); msleep(100); low(RTS)
script-run = once
color = 13

[usb devices]
pattern = usb([0-9]*)
device = /dev/ttyUSB%s
color = 14
```

To use a specific sub-configuration by name simply start tio like so:
```
$ tio rpi3
```
Or by pattern match:
```
$ tio usb12
```

Another more elaborate configuration file example is available [here](examples/config/config).

## 4. Installation

### 4.1 Installation using package manager (Linux)

Packages for various GNU/Linux distributions are available. Please consult your
package manager tool to find and install tio.

If you would like to see tio included in your favorite distribution, please
reach out to its package maintainers team.

### 4.2 Installation using snap (Linux)

Install latest stable version:
```
$ snap install tio --classic
```

Note: Classic confinement is currently required due to limitations of the snapcraft framework.
See [Issue #187](https://github.com/tio/tio/issues/187) for discussion.

### 4.3 Installation using brew (MacOS, Linux)

If you have [brew](http://brew.sh) installed:
```
$ brew install tio
```

### 4.4 Installation using MSYS2 (Windows)

If you have [MSYS2](https://www.msys2.org) installed:
```
$ pacman -S tio
```

### 4.5 Installation from source

The latest source releases can be found [here](https://github.com/tio/tio/releases).

Install steps:
```
$ meson setup build
$ meson compile -C build
$ meson install -C build
```

See meson\_options.txt for tio specific build options.

Note: The meson install steps may differ depending on your specific system.

### 4.6 Known issues

Getting permission access errors trying to open your serial device?

Add your user to the group which allows serial device access. For example, to add your user to the 'dialout' group do:
```
$ sudo usermod -a -G dialout <username>
```


## 5. Contributing

This is an open source project - all contributions (bug reports, code, doc,
ideas, etc.) are welcome.

Please use the github issue tracker and pull request features.

Also, if you find this free open source software useful please feel free to
consider making a donation of your choice:

[![Donate](images/paypal.png)](https://www.paypal.me/lundmar)


## 6. Support

Submit bug reports via GitHub: https://github.com/tio/tio/issues


## 7. Website

Visit [tio.github.io](https://tio.github.io)


## 8. License

tio is GPLv2+. See LICENSE file for more details.


## 9. Authors

Maintained by Martin Lund \<martin.lund@keep-it-simple.com>

See the AUTHORS file for full list of contributors.
