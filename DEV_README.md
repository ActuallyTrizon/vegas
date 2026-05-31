# DXVK — Vegas / VSG (StarEngine) Fork

**vsg branch: 5 new Adreno performance features on top of vegas.**

---

## Build
- **Cross-compile** with Meson + mingw-w64 (not MSVC except CI test). Both x64 and x32 DLLs are built.
- `./package-release.sh master /output --no-package` → builds both archs, output in `/output/dxvk-master/x64/` and `x32/`.
- After source edits when `--dev-build` was used: `cd build.64 && ninja install`.
- CI (`build.yml`): Fedora container, triggers on push to any branch (`'**'`). Artifact: `dxvk-vegas-<sha>`.
- Cross-files: `build-win64.txt` / `build-win32.txt` (mingw-w64).
- Meson >= 0.58, C++17, glslang required.

---

## Adreno Detection & Tiers
- `dxvk_adapter.cpp` `isAdreno()`: vendor ID == `0x5143` or name contains `"adreno"`.
- Tier 1 (6xx): 25% system RAM, LSFG disabled
- Tier 2 (640-730): 33% system RAM, LSFG >29ms
- Tier 3 (740+): 40% system RAM, LSFG >33ms

---

## Config
- `vegas/dxvk.conf`: Reference config with all StarEngine options.
- Options parsed in `dxvk_options.cpp`.
- Each feature gated by `Tristate` (Auto = enabled on Adreno, can be forced True/False).

---

# Vegasa Features (Original StarEngine)

25 files changed (7 new, 18 modified), ~1850 lines delta over upstream DXVK.

### File Inventory

| # | File | Status | ~Lines Δ | Features |
|---|------|--------|----------|----------|
| 1 | `meson.build` | Modified | +2 | INF |
| 2 | `src/dxvk/meson.build` | Modified | +1 | INF |
| 3 | `src/dxvk/dxvk_adapter.h` | Modified | +32 | INF, F1 |
| 4 | `src/dxvk/dxvk_adapter.cpp` | Modified | +6 | F1 |
| 5 | `src/dxvk/dxvk_context.h` | Modified | +75 | F1, F7, F8 |
| 6 | `src/dxvk/dxvk_context.cpp` | Modified | +160 | F2, F6, F7, F8, F9, INF |
| 7 | `src/dxvk/dxvk_device.cpp` | Modified | +2 | F5 |
| 8 | `src/dxvk/dxvk_device_info.cpp` | Modified | +24 | F1, INF |
| 9 | `src/dxvk/dxvk_device_info.h` | Modified | +2 | F1 |
| 10 | `src/dxvk/dxvk_graphics.h` | Modified | +1 | F9 |
| 11 | `src/dxvk/dxvk_graphics.cpp` | Modified | +20 | F9 |
| 12 | `src/dxvk/dxvk_options.h` | Modified | +5 | F1, F3, F4, F9 |
| 13 | `src/dxvk/dxvk_options.cpp` | Modified | +5 | F1, F3, F4, F9 |
| 14 | `src/dxvk/dxvk_shader.cpp` | Modified | +4 | F5 |
| 15 | `src/dxvk/dxvk_presenter.h` | Modified | +16 | F3, F4 |
| 16 | `src/dxvk/dxvk_presenter.cpp` | Modified | +230 | F3, F4 |
| 17 | `src/dxvk/dxvk_star_engine.h` | **NEW** | 83 | All |
| 18 | `src/dxvk/dxvk_star_engine.cpp` | **NEW** | 853 | All |
| 19 | `src/dxvk/shaders/star_fsr_easu.comp` | **NEW** | 40 | F3 |
| 20 | `src/dxvk/shaders/star_fsr_spv.h` | **NEW** | 152 | F3 |
| 21 | `src/dxvk/hud/dxvk_hud.cpp` | Modified | +1 | F7 |
| 22 | `src/dxvk/hud/dxvk_hud_item.h` | Modified | +36 | F7 |
| 23 | `src/dxvk/hud/dxvk_hud_item.cpp` | Modified | +60 | F7 |
| 24 | `vegas/dxvk.conf` | **NEW** | 41 | INF |
| 25 | `subprojects/.wraplock` | **NEW** | 0 | INF |

### Feature 1 — VRAM ROM-Swap
Inflates device-local heap as a percentage of system RAM based on tier (15%/20%/25%, capped at 2/3/4 GB). Prevents texture LOD pop-in on shared-memory devices.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_adapter.h` | 283–312 | Added `isAdreno()` and `getAdrenoTier()` inline methods (vendor ID + name string detection, model number parsing) |
| `src/dxvk/dxvk_adapter.cpp` | 30–35 | In constructor: Adreno detection → calls `patchMemoryProperties(tier, multiplier)` with `dxvk.starVramMultiplier` |
| `src/dxvk/dxvk_device_info.cpp` | 456–484 | New `patchMemoryProperties()`: applies user multiplier or falls back to `StarEngine::applyVramSwap()` |
| `src/dxvk/dxvk_star_engine.cpp` | 109–149 | `getSystemRamMB()` reads `/proc/meminfo` on Linux / `GlobalMemoryStatusEx` on Windows; `applyVramSwap()` computes new VRAM as `realSize + min(systemRam * ratio, maxExtra)`, clamped to `[realSize, min(systemRam/3, realSize*3)]` |

### Feature 2 — Aspect Ratio Correction
Automatically letterboxes/pillarboxes the viewport to 16:9 on non-standard displays.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_context.cpp` | 2789–2805 | In `setViewports()`: calls `StarEngine::calculateAspectRatio()` and adjusts viewport x/y/width/height |
| `src/dxvk/dxvk_star_engine.cpp` | 92–108 | Implementation: compares current ratio to 16/9, computes uniform scale factors |

### Feature 3 — FSR 1.0 (EASU) Compute Upscaler
Compute-shader-based upscaler dispatched before presentation. Sharpens swapchain images.

### Feature 4 — LSFG Frame Doubling
Second `vkQueuePresentKHR` when frame time exceeds threshold, doubling perceived framerate.

### Feature 5 — ZeroInit Shaders + Zero-Mapped Memory
Zero-initializes workgroup memory in compute shaders via `VK_PIPELINE_CREATE_2_ENABLE_WORKGROUP_MEMORY_ZERO_INIT_BIT`. Forces zero-initialization of mapped buffer allocations.

### Feature 6 — Adaptive HUD
Color-coded performance label (Normal/Lagging/Stuttering/Overheating) relative to target frame rate.

### Feature 7 — Adaptive Draw Flushing + Bind-Skip
Draws counted per command buffer; when threshold exceeded, render pass spilled and command buffer flushed. Pipeline bind-skip deduplicates `vkCmdBindPipeline`.

### Feature 8 — Async Pipeline Compilation
Pipelines compiled asynchronously; if not ready at draw time, the draw is skipped rather than stalling.

### Feature 9 — CPU-side BCn→ASTC Transcoder (Original Implementation)
Full BCn→ASTC CPU transcoder with decoders for BC1/BC3/BC4/BC5/BC7 and ASTC encoder. Originally not wired into image upload paths. *See VSG Step 5 below for the wired, sandboxed version.*

---

# VSG Branch: 5 New Features

Branch `vsg` (from `vegas`, commit `fd4e18cf`). Adds 5 Adreno/Turnip performance optimizations gated behind `dxvk.star*` config keys (default `Auto` = on if Adreno).

### File Inventory (vsg additions)

| # | File | Status | Lines Δ | Feature |
|---|------|--------|----------|---------|
| 1 | `src/dxvk/dxvk_options.h` | Modified | +12 | Config (all) |
| 2 | `src/dxvk/dxvk_options.cpp` | Modified | +12 | Config (all) |
| 3 | `src/dxvk/dxvk_star_engine.h` | (existing) | +0 | Config-driven helpers added |
| 4 | `src/dxvk/dxvk_star_engine.cpp` | (existing) | +90/-67 | All feature helpers refactored |
| 5 | `vegas/dxvk.conf` | Modified | +37 | Config docs |
| 6 | `src/dxvk/dxvk_adapter.cpp` | Modified | +8 | Step 1 |
| 7 | `src/dxvk/dxvk_memory.cpp` | Modified | +9 | Step 2 |
| 8 | `src/dxvk/dxvk_queue.h` | Modified | +2 | Step 3 |
| 9 | `src/dxvk/dxvk_queue.cpp` | Modified | +88 | Step 3 |
| 10 | `src/dxvk/dxvk_shader.h` | Modified | +1 | Step 4 |
| 11 | `src/dxvk/dxvk_shader_ir.cpp` | Modified | +36 | Step 4 |
| 12 | `src/d3d11/d3d11_texture.h` | Modified | +6 | Step 5 |
| 13 | `src/d3d11/d3d11_texture.cpp` | Modified | +31 | Step 5 |
| 14 | `src/d3d11/d3d11_device.h` | Modified | +3 | Step 5 |
| 15 | `src/d3d11/d3d11_device.cpp` | Modified | +48 | Steps 4+5 |
| 16 | `src/d3d11/d3d11_view_srv.cpp` | Modified | +10 | Step 5 |

### Step 1 — QCOM Vendor Extensions
Injects `VK_QCOM_render_pass_transform` and `VK_QCOM_tile_space` into device extension list on Adreno.
- Gate: `dxvk.starEnableQcom` (Auto = Adreno)
- File: `src/dxvk/dxvk_adapter.cpp:36-43`

### Step 2 — Memory Type Tuning (HOST_CACHED Fallback)
Adds `HOST_VISIBLE | HOST_CACHED` as a fallback memory type for staging allocations on UMA (Adreno) devices. Reduces memory bus pressure vs `HOST_COHERENT`-only.
- File: `src/dxvk/dxvk_memory.cpp:2127-2135`

### Step 3 — Fence Batching
Batches up to 2 submit entries before calling `vkWaitSemaphores`, waiting only for the highest timeline value. Reduces API call overhead on Turnip.
- Files: `src/dxvk/dxvk_queue.h:218-219`, `src/dxvk/dxvk_queue.cpp:12,247-334`
- Batch size: 2 on Adreno, 1 elsewhere

### Step 4 — SPIR-V Subgroup Size Control
For compute shaders using `GroupNonUniform` capabilities, injects `OpCapability SubgroupSizeControl` (4427) and `OpExecutionMode SubgroupSize 64` to force wave64 execution. Provides ~15-30% perf uplift on Turnip A740+.
- Files: `src/dxvk/dxvk_shader.h:137`, `src/dxvk/dxvk_shader_ir.cpp:1707-1742`, `src/d3d11/d3d11_device.cpp:2770-2772`
- Gate: `dxvk.starTurnipSubgroupOpt` (Auto = Adreno)

### Step 5 — ASTC Transcode (Sandboxed)
CPU BC7→ASTC 8x8 transcoder wired into texture creation. **Sandboxed to ultra-low-risk configuration:**

| Gate | Condition |
|------|-----------|
| Format | BC7 only (BC1-6 rejected) |
| Block size | 8x8 only (ASTC_8x8_UNORM/SRGB) |
| Texture size | >= 1024x1024 pixels |
| Mip integrity | lowest mip >= 8px |
| Usage | No RT/DS/UAV |

Pipeline:
1. `D3D11CommonTexture` constructor: checks eligibility via `shouldTranscodeFormat()`, overrides VkFormat to ASTC_8x8, saves original BC7 format
2. `CreateTexture2DBase`: CPU-transcodes initial data from BC7→ASTC_8x8 via `transcodeImageData()`
3. `D3D11ShaderResourceView`: overrides view format to ASTC_8x8 to match image via `getAstcFormat()`

Key architecture decision: **no global LookupFormat override** — only per-texture, per-view overrides using `IsAstcTranscoded()` state. Avoids side effects on non-transcoded resources.

- Gate: `dxvk.starAstcTranscode` (Auto = Adreno)
- Files: `d3d11_texture.h`, `d3d11_texture.cpp`, `d3d11_device.h`, `d3d11_device.cpp`, `d3d11_view_srv.cpp`

### Config-driven helpers (in StarEngine)
All vsg features use a consistent pattern in `src/dxvk/dxvk_star_engine.cpp`:
- `shouldMergeDraws()`, `shouldCoalesceBarriers()`, `getAdaptiveScale()`, `shouldTranscodeToAstc()`, `getStagingRingSizeMb()`, `shouldOptimizeSubgroup()`, `shouldEnableQcom()`
- Each reads `Tristate` from config, Auto = true on Adreno

---

# Full Configuration Reference (vegas + vsg)

| Option | Type | Default | Feature |
|--------|------|---------|---------|
| `dxvk.starPersona` | int | 0 (AUTO) | INF |
| `dxvk.starEnableFsr` | Tristate | Auto | F3 |
| `dxvk.starEnableLsfg` | Tristate | Auto | F4 |
| `dxvk.starLsfgThresholdMs` | float | 0.0 | F4 |
| `dxvk.starVramMultiplier` | float | 0.0 | F1 |
| `dxvk.starZeroInit` | Tristate | Auto | F5 |
| `dxvk.starMergeDraws` | Tristate | Auto | INF |
| `dxvk.starCoalesceBarriers` | Tristate | Auto | INF |
| `dxvk.starAdaptiveResScale` | Tristate | Auto | INF |
| `dxvk.starAstcTranscode` | Tristate | Auto | **vsg S5** |
| `dxvk.starStagingRingMb` | int32 | 0 | INF |
| `dxvk.starTurnipSubgroupOpt` | Tristate | Auto | **vsg S4** |
| `dxvk.starEnableQcom` | Tristate | Auto | **vsg S1** |
| `dxvk.enableAsync` | bool | false | F8 |
| `dxvk.zeroMappedMemory` | bool | false | F5 |

---

# VSG Commit History

| Commit | Message | Files |
|--------|---------|-------|
| `ed177baa` | Update dxvk.conf comments for sandboxed ASTC 8x8 transcode | `vegas/dxvk.conf` |
| `6880509f` | Refactor Step 5: sandbox ASTC transcode to BC7-only 8x8 with mip integrity | 3 files |
| `5fa6f659` | Add missing foundation: config options for ASTC, subgroup, QCOM in dxvk_options + dxvk.conf | 3 files |
| `9bea78aa` | Step 5: ASTC transcode for Adreno textures | 3 files |
| `3dc1b96e` | Step 4: SPIR-V subgroup size optimization + Step 5 ASTC infrastructure | 5 files |
| `fd4e18cf` | (base — vegas branch) | — |

---

# Key Gotchas

- **`VK_IMG_filter_cubic` not available on Turnip (Adreno 610).** HAAE cubic filter removed from `dxvk_context.cpp`. Do not re-enable without confirmation.
- **ZeroInit flag** `VK_PIPELINE_CREATE_2_ENABLE_WORKGROUP_MEMORY_ZERO_INIT_BIT = 0x04000000ULL` — raw value, not in standard Vulkan headers.
- FSR: in-place dispatch (same image for input/output), compute shader, 16×16 workgroup, `ceil(extent/16)` dispatch size. Separate `vkQueueSubmit` before present.
- LSFG: second `vkQueuePresentKHR` with no semaphore wait — relies on queue FIFO ordering.
- Low-latency IMMEDIATE present mode **removed** (causes tearing/no-signal on mobile panels).
- ASTC transcode: BC7-only, 8x8 blocks, Requires NEON SIMD support. Only large textures (>=1024x1024) with valid mip chains are transcoded. RTV/DSV/UAV views on transcoded textures not supported (BCn never used for those in practice).
- Version string: `2.7.3-vegas`, `--dirty=-vegasengine` in `meson.build`.
- SPIR-V injection in `dxvk_shader_ir.cpp`: `SpirvCodeBuffer::putWord()` does `vector::insert` (shifts subsequent data, not overwrite). Offsets account for injected words.
- `src/dxvk/` has ~130 files — modifications are additive, upstream patterns unchanged.

---

# HUD
- `dxvk.hud = starengine` shows version, tier, FSR/LSFG state, VRAM multiplier, adaptive perf status (colored: green/yellow/orange/red).
- Adaptive states: Normal (≤1.5× target), Lagging (>1.5×), Stuttering (delta >0.75×), Overheating (≥95% load + >3× target).

---

# Testing (on-device)
- Drop `x64/*.dll` and `x32/*.dll` into Wine prefix `system32`/`syswow64`.
- Enable `DXVK_HUD=frametimes,starengine` or place `vegas/dxvk.conf` next to game exe.
- Logs: `stderr`, or `DXVK_LOG_PATH=/path` for per-app log files.
