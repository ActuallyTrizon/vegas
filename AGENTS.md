# DXVK — Vegas (StarEngine) Fork

## Build
- **Cross-compile** with Meson + mingw-w64 (not MSVC except CI test). Both x64 and x32 DLLs are built.
- `./package-release.sh master /output --no-package` → builds both archs, output in `/output/dxvk-master/x64/` and `x32/`.
- After source edits when `--dev-build` was used: `cd build.64 && ninja install`.
- CI (`build.yml`): Fedora container, triggers on push to any branch (`'**'`). Artifact: `dxvk-vegas-<sha>`.
- Cross-files: `build-win64.txt` / `build-win32.txt` (mingw-w64).
- Meson >= 0.58, C++17, glslang required.

## StarEngine Features
9 features gated behind `dxvk.star*` config keys (default `Auto` = on if Adreno). Source: `src/dxvk/dxvk_star_engine.h/.cpp`.

| Feature | Config key | Key file(s) |
|---------|-----------|-------------|
| VRAM ROM-Swap | `dxvk.starVramMultiplier` | `dxvk_device_info.cpp` calls `applyVramSwap` |
| Aspect Ratio | (none) | `dxvk_context.cpp` `setViewports` calls `calculateAspectRatio` → 16:9 letterbox |
| FSR 1.0 EASU | `dxvk.starEnableFsr` | `dxvk_presenter.cpp` `initFsr/dispatchFsr`; SPIRV in `shaders/star_fsr_spv.h` |
| LSFG Frame Doubling | `dxvk.starEnableLsfg`, `dxvk.starLsfgThresholdMs` | `dxvk_presenter.cpp` second `vkQueuePresentKHR` |
| Zero-Mapped Memory | `dxvk.zeroMappedMemory` | `dxvk_device.cpp` auto-enables on Adreno |
| ZeroInit Shaders | (none) | `dxvk_shader.cpp` uses raw `0x04000000ULL` flag on compute pipelines |
| HAAE (cubic filter) | (none) | `dxvk_context.cpp` — **cubic filter disabled** (`VK_IMG_filter_cubic` broken on Turnip) |
| Adaptive HUD | `dxvk.hud = starengine` | `hud/dxvk_hud_item.cpp` `HudStarEngineItem` |
| ASTC Transcode | (planned, not implemented) | `shouldTranscodeFormat` stub in `star_engine.cpp` |

## Adreno Detection & Tiers
- `dxvk_adapter.cpp` `isAdreno()`: vendor ID == `0x5143` or name contains `"adreno"`.
- Tier 1 (6xx): 25% system RAM, LSFG disabled
- Tier 2 (640-730): 33% system RAM, LSFG >29ms
- Tier 3 (740+): 40% system RAM, LSFG >33ms

## Config
- `dxvk.conf` at root: upstream defaults (no StarEngine options).
- `vegas/dxvk.conf`: StarEngine reference config (FSR, LSFG, VRAM, HUD).
- StarEngine options parsed in `dxvk_options.cpp` (`starEnableFsr`, `starEnableLsfg`, `starLsfgThresholdMs`, `starVramMultiplier`).

## Key Gotchas
- **`VK_IMG_filter_cubic` not available on Turnip (Adreno 610).** HAAE cubic filter removed from `dxvk_context.cpp`. Do not re-enable without confirmation.
- **ZeroInit flag** `VK_PIPELINE_CREATE_2_ENABLE_WORKGROUP_MEMORY_ZERO_INIT_BIT = 0x04000000ULL` — raw value, not in standard Vulkan headers.
- FSR: in-place dispatch (same image for input/output), compute shader, 16×16 workgroup, `ceil(extent/16)` dispatch size. Separate `vkQueueSubmit` before present.
- LSFG: second `vkQueuePresentKHR` with no semaphore wait — relies on queue FIFO ordering.
- Low-latency IMMEDIATE present mode **removed** (causes tearing/no-signal on mobile panels).
- Version string: `2.7.3-vegas`, `--dirty=-vegasengine` in `meson.build`.
- `src/dxvk/` has ~130 files — StarEngine modifications are additive, upstream patterns unchanged.

## HUD
- `dxvk.hud = starengine` shows version, tier, FSR/LSFG state, VRAM multiplier, adaptive perf status (colored: green/yellow/orange/red).
- Adaptive states: Normal (≤1.5× target), Lagging (>1.5×), Stuttering (delta >0.75×), Overheating (≥95% load + >3× target).

## Testing (on-device)
- Drop `x64/*.dll` and `x32/*.dll` into Wine prefix `system32`/`syswow64`.
- Enable `DXVK_HUD=frametimes,starengine` or place `dxvk.conf` next to game exe.
- Logs: `stderr`, or `DXVK_LOG_PATH=/path` for per-app log files.
