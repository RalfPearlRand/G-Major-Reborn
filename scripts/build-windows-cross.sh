#!/usr/bin/env bash
# G-Major Reborn - cross-compile Windows x64 binaries on Linux with llvm-mingw + JUCE 7.0.12
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${LLVM_MINGW_ROOT:?set LLVM_MINGW_ROOT to an llvm-mingw (ucrt) toolchain}"
: "${JUCE_DIR:?set JUCE_DIR to a JUCE 7.0.12 checkout (JUCE 8 dropped MinGW)}"
BUILD="${BUILD_DIR:-$ROOT/build-win}"
export LLVM_MINGW_ROOT
# JUCE runs the Windows-only `attrib +s` on the VST3 bundle (folder icon); a no-op on the build host,
# the installer sets the attributes on the target machine.
SHIM="$(mktemp -d)"; trap 'rm -rf "$SHIM"' EXIT
printf '#!/bin/sh\nexit 0\n' > "$SHIM/attrib"; chmod +x "$SHIM/attrib"; export PATH="$SHIM:$PATH"
# juceaide is compiled for the host; use declaration stubs if X11 extension headers are missing
if [ ! -e /usr/include/X11/extensions/Xrandr.h ]; then export CPATH="$ROOT/cmake/host-x11-stubs${CPATH:+:$CPATH}"; fi
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/llvm-mingw-x64.cmake" -DGMR_JUCE_DIR="$JUCE_DIR" ${ASIO_SDK:+-DGMR_ASIO_SDK_DIR="$ASIO_SDK"}
cmake --build "$BUILD" --parallel
echo "Artifacts in $BUILD/plugin/GMajorReborn_artefacts/Release"
