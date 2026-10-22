#!/usr/bin/env bash
# Builds GMajorReborn-Setup.exe from a finished Windows build (see build-windows-cross.sh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${LLVM_MINGW_ROOT:?set LLVM_MINGW_ROOT}"
BUILD="${BUILD_DIR:-$ROOT/build-win}"
OUT="${OUT_DIR:-$ROOT/dist}"
VERSION="$(sed -n 's/^project(GMajorReborn VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
ART="$BUILD/plugin/GMajorReborn_artefacts/Release"
T="$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32"
STAGE="$(mktemp -d)"; GEN="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$GEN"' EXIT
mkdir -p "$STAGE/app/Standalone" "$STAGE/app/tools" "$STAGE/app/presets" "$STAGE/app/docs" "$STAGE/vst3" "$STAGE/userpresets/presets"
cp "$ART/Standalone/G-Major Reborn.exe" "$STAGE/app/Standalone/"
cp -r "$ART/VST3/G-Major Reborn.vst3" "$STAGE/vst3/"
for r in "$BUILD"/plugin/GMajor*Rack_artefacts/Release/VST3/*.vst3; do [ -e "$r" ] && cp -r "$r" "$STAGE/vst3/"; done
cp "$BUILD/gmr_render.exe" "$BUILD/gmr_tests.exe" "$STAGE/app/tools/"
cp "$ROOT"/presets/*.syx "$STAGE/app/presets/"; cp "$ROOT"/presets/*.syx "$STAGE/userpresets/presets/"
cp "$ROOT"/{README.md,INSTALL.md,KNOWN_DIFFERENCES.md,RECONSTRUCTION_REPORT.md,LICENSE.md} "$STAGE/app/docs/"
cp "$ROOT"/docs/*.md "$STAGE/app/docs/"
"$LLVM_MINGW_ROOT/bin/llvm-strip" "$STAGE/app/Standalone/"*.exe "$STAGE/app/tools/"*.exe "$STAGE"/vst3/*.vst3/Contents/x86_64-win/*.vst3
python3 -I "$ROOT/installer/gen_payload.py" "$STAGE" "$GEN" "$VERSION" "$ROOT/resources/icon.ico" "$ROOT/installer/setup.manifest"
"$T-windres" -i "$GEN/payload.rc" -o "$GEN/payload.o"
mkdir -p "$OUT"
"$T-clang++" -std=c++17 -O2 -Wall -Wextra -municode -mwindows -static -I"$GEN" "$ROOT/installer/Installer.cpp" "$GEN/payload.o" \
    -lcomctl32 -lole32 -lshell32 -luuid -ladvapi32 -o "$OUT/GMajorReborn-$VERSION-Setup-x64.exe"
"$LLVM_MINGW_ROOT/bin/llvm-strip" "$OUT/GMajorReborn-$VERSION-Setup-x64.exe"
ls -la "$OUT/GMajorReborn-$VERSION-Setup-x64.exe"
