#!/bin/bash
# STAR ENGINE: NATIVE BUILDER (v21.0.7)
set -e

SOURCE_DIR="/c/DXVK/dxvk-source/dxvk"
BUILD_DIR="/c/DXVK/build.ucrt64"
MESON_BIN="/c/Users/HomePC/AppData/Local/Python/pythoncore-3.14-64/Scripts/meson.exe"

cd "$SOURCE_DIR"

echo "🧹 Pre-Build: Clearing artifacts..."
rm -rf "$BUILD_DIR"
export PATH="/ucrt64/bin:/usr/bin:$PATH"

echo "⚔️ Phase 1: Native Configuration..."
# CC/CXX forced to UCRT64 GCC
CC=gcc CXX=g++ "$MESON_BIN" setup "$BUILD_DIR" . \
    --buildtype release \
    --strip \
    -Dcpp_args="-DVK_USE_PLATFORM_WIN32_KHR=1 -D_WIN32_WINNT=0x0A00" \
    -Dc_args="-DVK_USE_PLATFORM_WIN32_KHR=1 -D_WIN32_WINNT=0x0A00"

echo "⚔️ Phase 2: Compiling..."
ninja -C "$BUILD_DIR"

echo "📂 Phase 3: Collecting DLLs..."
mkdir -p "/c/DXVK/Final_Release_Star"
find "$BUILD_DIR" -name "*.dll" -exec cp {} "/c/DXVK/Final_Release_Star/" \;

echo "✨ SUCCESS! DLLs ready in C:/DXVK/Final_Release_Star"