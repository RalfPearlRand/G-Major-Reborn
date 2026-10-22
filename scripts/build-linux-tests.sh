#!/usr/bin/env bash
# Core tests (+ optional sanitizers) on Linux/macOS, no JUCE required
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ROOT" -B "$ROOT/build-tests" -DCMAKE_BUILD_TYPE=Release -DGMR_BUILD_PLUGIN=OFF
cmake --build "$ROOT/build-tests" --parallel
ctest --test-dir "$ROOT/build-tests" --output-on-failure
if [ "${SANITIZE:-0}" = 1 ]; then
  cmake -S "$ROOT" -B "$ROOT/build-asan" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGMR_BUILD_PLUGIN=OFF \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
  cmake --build "$ROOT/build-asan" --parallel && "$ROOT/build-asan/gmr_tests"
fi
