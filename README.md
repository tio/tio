# tio - a simple serial terminal I/O tool

[![CircleCI](https://circleci.com/gh/tio/tio/tree/master.svg?style=shield)](https://circleci.com/gh/tio/tio/tree/master)
[![tio](https://snapcraft.io/tio/badge.svg)](https://snapcraft.io/tio)
[![Packaging status](https://repology.org/badge/tiny-repos/tio.svg)](https://repology.org/project/tio/versions)

## 1. Introduction

tio is a simple serial terminal tool which features a straightforward
command-line interface to easily connect to TTY devices for basic I/O
operations.

<p align="center">
<img src="images/tio-demo.gif">
</p>

### 1.1 Motivation

To make a simpler serial terminal tool for talking with TTY devices with less
focus on classic terminal/modem features and more focus on the needs of
embedded developers and hackers.

## 2. Usage

The command-line interface is straightforward as reflected in the output from
'tio --help':
```
    Usage: tio [<options>] <tty-device>

    Options:
      -b, --baudrate <bps>        Baud rate (default: 115200)
      -d, --databits 5|6|7|8      Data bits (default: 8)
      -f, --flow hard|soft|none   Flow control (default: none)
      -s, --stopbits 1|2          Stop bits (default: 1)
      -p, --parity odd|even|none  Parity (default: none)
      -o, --output-delay <ms>     Output delay (default: 0)
      -n, --no-autoconnect        Disable automatic connect
      -e, --local-echo            Enable local echo
      -t, --timestamp[=<format>]  Enable timestamp (default: 24hour)
      -L, --list-devices          List available serial devices
      -l, --log <filename>        Log to file
      -m, --map <flags>           Map special characters
      -c, --color <0..255>        Colorize tio text
      -v, --version               Display version
      -h, --help                  Display help

    See the man page for more details.

    In session, press ctrl-t q to quit.
```

The only option which requires a bit of elaboration is perhaps the
`--no-autoconnect` option.

By default tio automatically connects to the provided device if present.  If
the device is not present, it will wait for it to appear and then connect. If
the connection is lost (eg. device is unplugged), it will wait for the device
to reappear and then reconnect. However, if the `--no-autoconnect` option is
provided, tio will exit if the device is not present or an established
connection is lost.

Tio supports various in session key commands. Press ctrl-t ? to list the
available key commands.

Tio also features full bash autocompletion support and configuration via ~/.tiorc.

See the tio man page for more details.


## 3. Installation

### 3.1 Installation using package manager
tio comes prepackaged for various GNU/Linux distributions. Please consult your package manager tool to find and install tio.

### 3.2 Installation using snap

Install latest stable version:
```
    $ snap install tio
```
Install bleeding edge:
```
    $ snap install tio --edge
```

### 3.3 Installation from source

The latest source releases can be found [here](https://github.com/tio/tio/releases).

Install steps:
```
    $ meson build
    $ meson compile -C build
    $ meson install -C build
```

See meson\_options.txt for tio specific build options.

Note: Please do no try to install from source if you are not familiar with
how to build stuff using meson.


## 4. Contributing

tio is open source. If you want to help out with the project please feel free
to join in.

All contributions (bug reports, code, doc, ideas, etc.) are welcome.

Please use the github issue tracker and pull request features.

Also, if you find this free open source software useful please feel free to
consider making a donation of your choice:

[![Donate](images/paypal.png)](https://www.paypal.me/lundmar)


## 5. Support

Submit bug reports via GitHub: https://github.com/tio/tio/issues


## 6. Website

Visit [tio.github.io](https://tio.github.io)


## 7. License

Tio is GPLv2+. See LICENSE file for more details.


## 8. Authors

Created by Martin Lund \<martin.lund@keep-it-simple.com>

See the AUTHORS file for full list of contributors.
