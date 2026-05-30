# StarEngine (Vegas Edition) — Developer Reference

**25 files changed (7 new, 18 modified), ~1850 lines delta over upstream DXVK.**

## File Inventory

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

---

## Feature-by-Feature Breakdown

### Feature 1 — VRAM ROM-Swap

Inflates device-local heap as a percentage of system RAM based on tier (15%/20%/25%, capped at 2/3/4 GB). Prevents texture LOD pop-in on shared-memory devices.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_adapter.h` | 283–312 | Added `isAdreno()` and `getAdrenoTier()` inline methods (vendor ID + name string detection, model number parsing) |
| `src/dxvk/dxvk_adapter.cpp` | 30–35 | In constructor: Adreno detection → calls `patchMemoryProperties(tier, multiplier)` with `dxvk.starVramMultiplier` |
| `src/dxvk/dxvk_device_info.cpp` | 456–484 | New `patchMemoryProperties()`: applies user multiplier or falls back to `StarEngine::applyVramSwap()` |
| `src/dxvk/dxvk_device_info.h` | 307, 329 | Declaration `patchMemoryProperties()`, new member `m_originalDeviceLocalSize` |
| `src/dxvk/dxvk_options.h` | 86 | `float starVramMultiplier = 0.0f` |
| `src/dxvk/dxvk_options.cpp` | 32 | Reads `dxvk.starVramMultiplier` |
| `src/dxvk/dxvk_star_engine.h` | 26–27 | `getSystemRamMB()`, `applyVramSwap()` declarations |
| `src/dxvk/dxvk_star_engine.cpp` | 109–149 | `getSystemRamMB()` reads `/proc/meminfo` on Linux / `GlobalMemoryStatusEx` on Windows; `applyVramSwap()` computes new VRAM as `realSize + min(systemRam * ratio, maxExtra)`, clamped to `[realSize, min(systemRam/3, realSize*3)]` |
| `vegas/dxvk.conf` | 24–26 | Config option |

---

### Feature 2 — Aspect Ratio Correction

Automatically letterboxes/pillarboxes the viewport to 16:9 on non-standard displays.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_context.cpp` | 2789–2805 | In `setViewports()`: calls `StarEngine::calculateAspectRatio()` and adjusts viewport x/y/width/height |
| `src/dxvk/dxvk_star_engine.h` | 25 | `calculateAspectRatio()` declaration |
| `src/dxvk/dxvk_star_engine.cpp` | 92–108 | Implementation: compares current ratio to 16/9, computes uniform scale factors |

---

### Feature 3 — FSR 1.0 (EASU) Compute Upscaler

Compute-shader-based upscaler dispatched before presentation. Sharpens swapchain images.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_options.h` | 77 | `Tristate starEnableFsr = Tristate::Auto` |
| `src/dxvk/dxvk_options.cpp` | 29 | Reads `dxvk.starEnableFsr` |
| `src/dxvk/dxvk_presenter.h` | 330–345 | Added FSR members: `m_fsrInitialized`, `StarFsr m_fsr`, `m_fsrCmdPool`, `m_fsrCmdBuffer`, `m_fsrDescPool`, `m_fsrDescSet`, `m_fsrImageViews`, `m_fsrComplete` semaphore; declarations for `initFsr()`, `destroyFsr()`, `dispatchFsr()` |
| `src/dxvk/dxvk_presenter.cpp` | 209–216 | FSR dispatch before present, chains `m_fsrComplete` semaphore into present wait |
| `src/dxvk/dxvk_presenter.cpp` | 786–798 | Checks `VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT` before enabling FSR |
| `src/dxvk/dxvk_presenter.cpp` | 877 | `initFsr()` call after swapchain creation |
| `src/dxvk/dxvk_presenter.cpp` | 1289 | `destroyFsr()` in swapchain cleanup |
| `src/dxvk/dxvk_presenter.cpp` | 1422–1519 | `initFsr()`: creates FSR pipeline, descriptor set, image views per swap image, completion semaphore |
| `src/dxvk/dxvk_presenter.cpp` | 1522–1539 | `destroyFsr()`: destroys all FSR Vulkan resources |
| `src/dxvk/dxvk_presenter.cpp` | 1542–1632 | `dispatchFsr()`: records pipeline barrier PRC→GEN, dispatches FSR compute, barrier back, submits with semaphore sync |
| `src/dxvk/dxvk_star_engine.h` | 29, 51–82 | `calculateFsrConstants()` declaration, `class StarFsr` with `init()/destroy()/dispatch()` and pipeline creation helpers |
| `src/dxvk/dxvk_star_engine.cpp` | 179–189 | `calculateFsrConstants()`: computes EASU constants from src/dst extents |
| `src/dxvk/dxvk_star_engine.cpp` | 759–851 | `StarFsr` implementation: shader module, descriptor set layout (2 storage images), pipeline layout, compute pipeline (with optional zero-init), dispatch |
| `src/dxvk/shaders/star_fsr_easu.comp` | 1–40 | GLSL compute shader — FSR 1.0 EASU with cubic interpolation |
| `src/dxvk/shaders/star_fsr_spv.h` | 1–152 | Pre-compiled SPIR-V binary for FSR EASU |
| `vegas/dxvk.conf` | 11–13 | Config option |

---

### Feature 4 — LSFG Frame Doubling

Second `vkQueuePresentKHR` when frame time exceeds threshold, doubling perceived framerate.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_options.h` | 80, 83 | `Tristate starEnableLsfg = Tristate::Auto`, `float starLsfgThresholdMs = 0.0f` |
| `src/dxvk/dxvk_options.cpp` | 30–31 | Reads `dxvk.starEnableLsfg` and `dxvk.starLsfgThresholdMs` |
| `src/dxvk/dxvk_presenter.h` | 339–340 | `bool m_frameGenEnabled`, `m_lastPresentTime` for frame delta measurement |
| `src/dxvk/dxvk_presenter.cpp` | 220–262 | After present: checks `starEnableLsfg`, computes frame time from `m_lastPresentTime`, compares against threshold (user or per-tier), calls second `vkQueuePresentKHR` with same image index |
| `src/dxvk/dxvk_star_engine.h` | 28 | `needsFrameGen()` declaration |
| `src/dxvk/dxvk_star_engine.cpp` | 173–178 | `needsFrameGen()`: tier 1 = disabled, tier 2 = >29ms, tier 3 = >33ms |
| `vegas/dxvk.conf` | 15–22 | Config options |

---

### Feature 5 — ZeroInit Shaders + Zero-Mapped Memory

Zero-initializes workgroup memory in compute shaders via `VK_PIPELINE_CREATE_2_ENABLE_WORKGROUP_MEMORY_ZERO_INIT_BIT`. Forces zero-initialization of mapped buffer allocations. Auto-enabled on Adreno.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_device.cpp` | 30–31 | In `DxvkDevice` constructor: auto-set `zeroMappedMemory = true` on Adreno unless user explicitly set `dxvk.zeroMappedMemory` |
| `src/dxvk/dxvk_shader.cpp` | 545–549 | In `createShaderModule()`: if Adreno and `shouldZeroInit(tier)`, sets `VK_PIPELINE_CREATE_2_ENABLE_WORKGROUP_MEMORY_ZERO_INIT_BIT` on compute pipelines |
| `src/dxvk/dxvk_star_engine.h` | 24 | `shouldZeroInit()` declaration |
| `src/dxvk/dxvk_star_engine.cpp` | 87–91 | `shouldZeroInit()`: returns `true` (always enabled) |

---

### Feature 6 — Adaptive HUD

Color-coded performance label (Normal/Lagging/Stuttering/Overheating) relative to target frame rate. Shown via `DXVK_HUD=starengine`.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/hud/dxvk_hud.cpp` | 30 | Registers `HudStarEngineItem("starengine")` into HUD item set |
| `src/dxvk/hud/dxvk_hud_item.h` | 740–775 | New `class HudStarEngineItem : public HudItem` with `update()`, `render()`, status string buffers |
| `src/dxvk/hud/dxvk_hud_item.cpp` | 1391–1463 | Constructor builds status line ("StarEngine v2.7.3-vegas [tier X]"), FSR/LSFG state; `update()` no-op; `render()` draws color-coded performance state (green/yellow/orange/red) |
| `src/dxvk/dxvk_star_engine.h` | 9–13, 30–32 | `StarPerformanceState` enum (Normal/Lagging/Stuttering/Overheating), `analyzePerformance()`, `getGraphColor()`, `getStatusString()` |
| `src/dxvk/dxvk_star_engine.cpp` | 190–233 | `analyzePerformance()`: compares frameTime/load/delta against target thresholds; `getGraphColor()`: maps state to 0x00FF00/0xFFFF00/0xFF8800/0xFF0000; `getStatusString()`: state→string |

---

### Feature 7 — Adaptive Draw Flushing + Bind-Skip

Draws are counted per command buffer. When count exceeds threshold (600/1200/2000 per tier), render pass is spilled and command buffer flushed. Pipeline bind-skip deduplicates redundant `vkCmdBindPipeline` calls. AdrenoGovernor dynamically tunes threshold based on GPU load and frame time.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_context.h` | 827–842 | New `StarProfile` struct (`enabled`, `initialized`, `allowBindSkip`, `drawThreshold`, `tier`, `lastBoundVkPipeline`); `std::atomic<uint32_t> m_drawsSinceSubmit{0}`; `initStarProfile()`, `checkAsyncCompilationCompat()` |
| `src/dxvk/dxvk_context.cpp` | 94–105 | Constructor: init `m_drawsSinceSubmit`, `m_starProfile` members, call `initStarProfile()` |
| `src/dxvk/dxvk_context.cpp` | 122–126 | `beginRecording()`: reset draw counter + pipeline tracking |
| `src/dxvk/dxvk_context.cpp` | 185–189 | `flushCommandList()`: reset all StarEngine state |
| `src/dxvk/dxvk_context.cpp` | 839–864, 950–977 | New `draw()` and `drawIndexed()`: check threshold, flush if exceeded, increment counter |
| `src/dxvk/dxvk_context.cpp` | 5334 | `spillRenderPass()`: reset `lastBoundVkPipeline` |
| `src/dxvk/dxvk_context.cpp` | 5899–5928 | `updateGraphicsPipeline()`: bind-skip — if same pipeline already bound and state not dirty, skip `vkCmdBindPipeline` |
| `src/dxvk/dxvk_context.cpp` | 9525–9564 | `initStarProfile()`: detect GPU, set tier, assign threshold from table `{600, 1200, 2000}` |
| `src/dxvk/dxvk_star_engine.h` | 20–23 | `initializeProfile()` (2 overloads), `tuneThreshold()` declarations |
| `src/dxvk/dxvk_star_engine.cpp` | 17–66 | `initializeProfile()`: Adreno detection via vendorID + name string, model number → tier, D3D9-specific thresholds `{1500, 3000, 5000}` |
| `src/dxvk/dxvk_star_engine.cpp` | 67–86 | `tuneThreshold()`: high load + high frame time → lower threshold (flush sooner), low load → raise to 8000 |

---

### Feature 8 — Async Pipeline Compilation

Pipelines compiled asynchronously on worker thread. If not ready at draw time, the draw call is skipped rather than stalling.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_options.h` | 15 | `bool enableAsync` |
| `src/dxvk/dxvk_options.cpp` | 24 | Reads `dxvk.enableAsync` |
| `src/dxvk/dxvk_context.h` | 1107 | `updateGraphicsPipeline()` signature: `bool updateGraphicsPipeline(bool async = false)` |
| `src/dxvk/dxvk_context.cpp` | 5899–5909 | In `updateGraphicsPipeline()`: if `useAsync` and pipeline not ready, `return false` |
| `src/dxvk/dxvk_context.cpp` | 7181–7183 | In `commitGraphicsState()`: passes `useAsync` through |
| `src/dxvk/dxvk_context.cpp` | 9567–9569 | `checkAsyncCompilationCompat()`: always returns true |
| `src/dxvk/dxvk_graphics.h` | 549–552 | `getPipelineHandle()`: `VkPipeline getPipelineHandle(const DxvkGraphicsPipelineStateInfo& state, bool async = false)` |
| `src/dxvk/dxvk_graphics.cpp` | 1069–1098 | `getPipelineHandle()`: if async and pipeline instance not found, return `VK_NULL_HANDLE` immediately (skip frame) instead of blocking |

---

### Feature 9 — ASTC Texture Compression *(Future)*

Full BCn→ASTC transcoding pipeline is implemented (decoders + encoder) but not yet wired into image upload paths.

| File | Lines | Change |
|------|-------|--------|
| `src/dxvk/dxvk_star_engine.h` | 33–48 | Declarations: `shouldTranscodeFormat()`, `formatIsBcn()`, `getAstcFormat()`, `transcodeImageData()` |
| `src/dxvk/dxvk_star_engine.cpp` | 234–342 | `formatIsBcn()`: checks 17 BCn format variants; `getAstcFormat()`: maps BC1→ASTC 6x6, BC2/3→5x5, BC4→6x6, BC5→5x5, BC7→5x5, BC6H→UNDEFINED (skipped HDR); `shouldTranscodeFormat()`: gates on usage (skip RT/depth/stencil), size (≥512KB), platform support |
| `src/dxvk/dxvk_star_engine.cpp` | 344–643 | Anonymous namespace: `decodeBC1()`, `decodeBC3()`, `decodeBC4()`, `decodeBC5()`, `decodeBC7()` decoders; `encodeAstcBlock()` with Morton-order texel scanning; `astcSetBits()`, `astcBlockMode()`, `astcBlockDims()` helpers |
| `src/dxvk/dxvk_star_engine.cpp` | 645–757 | `transcodeImageData()`: full pipeline — decode source BCn → RGBA8 → encode to ASTC per block |

---

## Commit History

| Commit | Message | Files |
|--------|---------|-------|
| `0eff670f` | StarEngine 2.7.3-vegas: Adreno performance suite | All core changes |
| `4c84a7ac` | FSR semaphore safety, VRAM limits, Turnip compat | `dxvk_presenter.*`, `dxvk_star_engine.*`, `dxvk_device_info.cpp` |
| `56a28ee1` | Remove cubic filter override — VK_IMG_filter_cubic not available on Turnip | `dxvk_context.cpp` (−3 lines) |
| `ca231c02` | Fix: use vkd dispatch table instead of raw vkCmd* calls | `dxvk_presenter.cpp`, `dxvk_star_engine.*` |
| `fe1accd6` | Fix FSR init: queueFamily, temp var for descriptorSetLayout | `dxvk_presenter.cpp` |
| `50f1acb4` | Remove dead loadStarConfig() | `dxvk_context.cpp` (−54 lines) |
| `fef4dc83` | Fix sign-compare warning in isAdreno() | `dxvk_adapter.h` |
| `cecf33f1` | Fix: use raw 0x04000000ULL for ZeroInit flag (mingw compat) | `dxvk_shader.cpp` |
| `8c293dba` | Fix stray closing brace in star_fsr_spv.h | `star_fsr_spv.h` |
| `b9739430` | Fix HUD: wrong struct in update() — make no-op | `dxvk_hud_item.cpp` |
| `87e0ddc5` | Fix VRAM under-report: never report less than real HW VRAM | `dxvk_star_engine.cpp` |
| `9a988fdb` | Wire StarEngine::applyGpuMask to dxvk.starPersona config option | `dxvk_device_info.cpp` |

---

## Configuration Reference

| Option | Type | Default | Feature |
|--------|------|---------|---------|
| `dxvk.starPersona` | int | 0 (AUTO) | INF |
| `dxvk.starEnableFsr` | Tristate | Auto | F3 |
| `dxvk.starEnableLsfg` | Tristate | Auto | F4 |
| `dxvk.starLsfgThresholdMs` | float | 0.0 | F4 |
| `dxvk.starVramMultiplier` | float | 0.0 | F1 |
| `dxvk.enableAsync` | bool | false | F8 |
| `dxvk.zeroMappedMemory` | bool | false | F5 |
| `dxgi.customVendorId` | int32 | -1 | INF |
| `dxgi.customDeviceId` | int32 | -1 | INF |
| `dxgi.customDeviceDesc` | string | "" | INF |
