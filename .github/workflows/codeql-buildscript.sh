#!/usr/bin/env bash

pip3 install meson -U
pip3 install ninja -U
meson setup build
meson compile -C build
