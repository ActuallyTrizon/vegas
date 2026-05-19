#!/bin/bash
# STAR ENGINE: NATIVE BUILDER (v21.0.0)
set -e

# Setup Paths
SOURCE_DIR="/c/DXVK/dxvk-source/dxvk"
BUILD_DIR="/c/DXVK/build.ucrt64"
MESON_BIN="/c/Users/HomePC/AppData/Local/Python/pythoncore-3.14-64/Scripts/meson.exe"
cd "$SOURCE_DIR"

echo "🧹 Pre-Build: Clearing artifacts..."
rm -rf "$BUILD_DIR"
export PATH="/ucrt64/bin:/usr/bin:$PATH"

echo "⚔️ Phase 1: Native Configuration..."
# Force GCC/G++ and target the Windows 10/11 API
CC=gcc CXX=g++ "$MESON_BIN" setup "$BUILD_DIR" . \
    --buildtype release \
    --strip \
    -Ddisplay_info=disabled \
    -Dcpp_args="-DVK_USE_PLATFORM_WIN32_KHR=1 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00" \
    -Dc_args="-DVK_USE_PLATFORM_WIN32_KHR=1 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00"

echo "⚔️ Phase 2: Compiling (Boss Fight Step 86)..."
ninja -C "$BUILD_DIR"

echo "📂 Phase 3: Collecting Star Engine DLLs..."
mkdir -p "/c/DXVK/StarEngine_Final_Release"
find "$BUILD_DIR" -name "*.dll" -exec cp {} "/c/DXVK/StarEngine_Final_Release/" \;

echo "------------------------------------------------------------"
echo "✨ SUCCESS! DLLs ready at: C:/DXVK/StarEngine_Final_Release"
echo "------------------------------------------------------------"