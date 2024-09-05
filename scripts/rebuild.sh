#!/bin/sh

if [ "$1" = "all" ]; then
  rm -rf build
  meson setup build
fi

meson compile -C build
meson install -C build
