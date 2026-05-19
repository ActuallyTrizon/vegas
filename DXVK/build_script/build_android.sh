# 1. ENTER THE SOURCE DIRECTORY
# Updated to use the 'dxvk' folder name
cd /c/DXVK/dxvk-source/dxvk

# 2. CLEAR THE PREVIOUS BUILD CACHE
# Ensures a clean configuration for the native UCRT64 toolchain
rm -rf /c/DXVK/build.ucrt64

# 3. CONFIGURE THE NATIVE WINDOWS BUILD
# We use your specific Meson path and target the Windows 10/11 API
/c/Users/HomePC/AppData/Local/Python/pythoncore-3.14-64/Scripts/meson.exe setup /c/DXVK/build.ucrt64 . \
    --buildtype release \
    --strip \
    -Dcpp_args="-DVK_USE_PLATFORM_WIN32_KHR=1 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00" \
    -Dc_args="-DVK_USE_PLATFORM_WIN32_KHR=1 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00"

# 4. START THE COMPILATION
# Compile safely with restricted parallel processing
ninja -j 2 -C /c/DXVK/build.ucrt64

# 5. COLLECT THE GENERATED DLLs
# Moves the final Star Engine binaries to your release folder
mkdir -p /c/DXVK/StarEngine_Release
find /c/DXVK/build.ucrt64 -name "*.dll" -exec cp {} /c/DXVK/StarEngine_Release/ \;

echo "------------------------------------------------------------"
echo "✨ NATIVE BUILD ATTEMPT COMPLETE"
echo "📂 DLLs (if successful): C:/DXVK/StarEngine_Release"
echo "------------------------------------------------------------"