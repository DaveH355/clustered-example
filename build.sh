#!/bin/bash

set -euo pipefail # exit on first error

CONFIG_FILE=".build-config"
DEFAULT_BUILD_TYPE="Debug"
DEFAULT_COMPILER="gcc"
RELOAD_ONLY=false

# Set to defaults which may be overridden by the config file
BUILD_TYPE="$DEFAULT_BUILD_TYPE"
COMPILER="$DEFAULT_COMPILER"

# Load configuration if it exists
if [ -f "$CONFIG_FILE" ]; then
  source "$CONFIG_FILE"
fi

# Check if the currently configured build directory exists. If not, reset to defaults.
BUILD_DIR="build_${BUILD_TYPE}_${COMPILER}"
if [ ! -d "$BUILD_DIR" ]; then
  echo "The build directory ${BUILD_DIR} no longer exists. Running with default options"
  BUILD_TYPE="$DEFAULT_BUILD_TYPE"
  COMPILER="$DEFAULT_COMPILER"
fi

print_usage() {
  echo "Usage: $0 [-b build_type] [-r] [-c compiler]"
  echo "  -b  Specify build type (Debug, Release)"
  echo "  -r  Reload CMake only, do not build or run"
  echo "  -c  Specify compiler (gcc, clang)"
  exit 1
}

# parse args
while getopts "b:rc:" opt; do
  case $opt in
    b) BUILD_TYPE="${OPTARG}" ;;
    r) RELOAD_ONLY=true ;;
    c) COMPILER="${OPTARG}" ;;
    *) print_usage ;;
  esac
done

BUILD_DIR="build_${BUILD_TYPE}_${COMPILER}"

# Save config to file
echo "BUILD_TYPE=\"${BUILD_TYPE}\"" > "$CONFIG_FILE"
echo "COMPILER=\"${COMPILER}\"" >> "$CONFIG_FILE"

if ! command -v cmake &> /dev/null; then
  echo "cmake could not be found, please install it."
  exit 1
fi

case "$COMPILER" in
  clang)
    export CC=clang
    export CXX=clang++
    ;;
  gcc)
    export CC=gcc
    export CXX=g++
    ;;
  *)
    echo "Invalid compiler: ${COMPILER}. Valid compilers are gcc, clang."
    exit 1
    ;;
esac

mkdir -p "${BUILD_DIR}"
pushd "${BUILD_DIR}"

# reload cmake
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" .. || { echo "cmake configuration failed"; popd; exit 1; }

if [ -f "compile_commands.json" ]; then
  cp compile_commands.json ../
else
  echo "No compile_commands.json found in the current build directory."
fi

if [ "$RELOAD_ONLY" = false ]; then
  cmake --build . || { echo "Build failed"; popd; exit 1; }

  # nvidia with valgrind (possible driver mem leaks, not as accurate)
  # export __GLX_VENDOR_LIBRARY_NAME=nvidia && export __NV_PRIME_RENDER_OFFLOAD=1 && valgrind --leak-check=full ./opengl_playground --quality=high

  # igpu with valgrind
  # valgrind --leak-check=full ./opengl_playground --quality=low
  
  # callgrind (run kcachegrind on output)
  # export __GLX_VENDOR_LIBRARY_NAME=nvidia && export __NV_PRIME_RENDER_OFFLOAD=1 && valgrind --tool=callgrind ./opengl_playground

  # nvidia 
  export __GLX_VENDOR_LIBRARY_NAME=nvidia && export __NV_PRIME_RENDER_OFFLOAD=1 && ./opengl_playground --quality=high

  # note: currently seeing igpu slowdown when fullscreen
  # ignore: could be anything. driver, gnome, glfw etc
  # ./opengl_playground --quality=low
  

fi

popd
