#!/bin/sh
set -e
cd "$(dirname "$0")"

CMAKE=cmake
if ! command -v cmake >/dev/null 2>&1; then
    CMAKE="/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
fi

"$CMAKE" -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
"$CMAKE" --build cmake-build-debug -j "$(sysctl -n hw.ncpu)"
cd RaylibLearning
./RaylibLearning
