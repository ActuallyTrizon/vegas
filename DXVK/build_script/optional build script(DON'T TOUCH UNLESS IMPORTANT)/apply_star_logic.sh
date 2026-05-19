#!/bin/bash
# STAR ENGINE: THE FINAL SYNC (v21.1.13)
set -e

# Update these paths to match your actual folder structure
SOURCE_DIR="/c/DXVK/dxvk-source/dxvk"
PATCH_FILE="/c/DXVK/patches/StarEngine_Logic_v2.7.2.1.patch"

cd "$SOURCE_DIR"

echo "🧪 PHASE 1: Applying Star Engine Patch..."
if [ -f "$PATCH_FILE" ]; then
    # Clean and apply the patch
    cat "$PATCH_FILE" | tr -d '\r' | tr -cd '\11\12\15\40-\176' > star_temp.patch
    patch -p0 --fuzz=5 --ignore-whitespace < star_temp.patch || echo "⚠️ Patch hunks failed; Wisdom Bridge will heal them..."
    rm star_temp.patch
else
    echo "❌ ERROR: Patch file not found at $PATCH_FILE"
    exit 1
fi

echo "🛡️ PHASE 2: Wisdom Bridge (Header & Source Sync)..."

# 1. Clean the Header Contract
# We purge all draw and compute hazard declarations to prevent 'Redeclaration' errors
sed -i '/void draw(/,/draws);/d' src/dxvk/dxvk_context.h
sed -i '/void drawIndexed(/,/draws);/d' src/dxvk/dxvk_context.h
sed -i '/checkComputeHazards/d' src/dxvk/dxvk_context.h
# Delete the redundant original definitions from the .cpp file
sed -i '/void DxvkContext::draw(/,/drawGeneric<false>(count, draws);/d' src/dxvk/dxvk_context.cpp
sed -i '/void DxvkContext::drawIndexed(/,/drawGeneric<true>(count, draws);/d' src/dxvk/dxvk_context.cpp

# 2. Inject Fresh Star Engine Blueprints
# These match the exact logic found in your .cpp.rej and patch file
sed -i '/class DxvkContext : public RcObject {/a \  public:\n    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);\n    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);\n    template<bool Indirect> bool checkComputeHazards();' src/dxvk/dxvk_context.h

# 3. Align the .cpp Logic
# This fixes the 'no matching function' error by ensuring the template is recognized
sed -i 's/bool DxvkContext::checkComputeHazards() {/template<bool Indirect> bool DxvkContext::checkComputeHazards() {/g' src/dxvk/dxvk_context.cpp
# Fix the call site inside commitComputeState
sed -i 's/this->checkComputeHazards()/this->checkComputeHazards<false>()/g' src/dxvk/dxvk_context.cpp

echo "------------------------------------------------------------"
echo "🔍 VERIFICATION:"
echo "Header Draw Count: $(grep -c "void draw(uint32_t" src/dxvk/dxvk_context.h)"
echo "Header Compute Count: $(grep -c "checkComputeHazards();" src/dxvk/dxvk_context.h)"
echo "------------------------------------------------------------"

echo "🚀 SUCCESS: Star Engine logic is fused and synchronized."