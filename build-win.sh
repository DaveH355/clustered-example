#!/bin/bash

# windows built script (run with git bash)

readonly BUILD_DIR="cmake_build_msvc"
readonly VCPKG_PATH="C:/dev/vcpkg"

if [[ ! -d $BUILD_DIR ]]; then
  (
    mkdir "$BUILD_DIR"
    cd "$BUILD_DIR" || exit

    # let cmake find the visual studio compilers and decide generator
    cmake -DCMAKE_TOOLCHAIN_FILE="${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake" ..
  )
fi

cd "$BUILD_DIR" || exit

# visual studio generator is multi config so we have to specify release variant here?
# https://stackoverflow.com/a/74077157/19633924
cmake --build . --target ALL_BUILD --config Release || exit

echo "build success!"

# cd Release || exit
# exec ./opengl_playground.exe



