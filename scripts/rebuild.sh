#!/bin/sh

# Find the full path to the meson binary
MESON=$(which meson)

# Clean the previous build directory
rm -rf build

# Set up the build configuration based on the argument
if [ "$1" = "all" ]; then
  $MESON setup build -Dinstall_libs=true -Dinstall_exec=true
elif [ "$1" = "libs" ]; then
  $MESON setup build -Dinstall_libs=true -Dinstall_exec=false
elif [ "$1" = "exec" ]; then
  $MESON setup build -Dinstall_libs=false -Dinstall_exec=true
else
  echo "Invalid option. Use 'all', 'libs', or 'exec'."
  exit 1
fi

# Compile the project before installing
$MESON compile -C build

# Install libraries and executables separately
if [ "$1" = "libs" ]; then
  echo "Installing libraries with sudo..."
  sudo "$MESON" install -C build
elif [ "$1" = "exec" ]; then
  echo "Installing executables (no sudo needed)..."
  $MESON install -C build
elif [ "$1" = "all" ]; then
  echo "Installing executables (no sudo needed)..."
  $MESON install -C build --only-changed --no-rebuild

  echo "Installing libraries with sudo..."
  sudo "$MESON" install -C build --only-changed --no-rebuild
fi
