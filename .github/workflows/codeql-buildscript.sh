#!/usr/bin/env bash

pip3 install meson -U
pip3 install ninja -U
sudo apt-get install -y liblua5.2-dev libglib2.0-dev
meson setup build
meson compile -C build
