#include "dxvk_star_engine.h"
#include "dxvk_device.h"
#include "dxvk_adapter.h"
#include <algorithm>
#include <string>
#include <cstring>
#include <cctype>
#include "shaders/star_fsr_spv.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace dxvk {

  void StarEngine::initializeProfile(uint32_t& threshold, bool& enabled, bool& bindSkip, uint32_t& tier, DxvkDevice* device) {
      if (device == nullptr || device->adapter() == nullptr) return;
      auto& props = device->adapter()->deviceProperties().core.properties;

      bool isAdreno = (props.vendorID == 0x5143);
      if (!isAdreno) {
          constexpr const char* adrenoStr = "adreno";
          for (size_t i = 0; i < VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 6 && props.deviceName[i] != '\0'; ++i) {
              size_t j = 0;
              for (; j < 6; ++j) {
                  if (std::tolower(static_cast<unsigned char>(props.deviceName[i + j])) != adrenoStr[j]) break;
              }
              if (j == 6) { isAdreno = true; break; }
          }
      }

      if (isAdreno) {
          enabled = true;
          bindSkip = true;
          tier = 2;
          for (const char* c = props.deviceName; *c; ++c) {
              if (*c >= '0' && *c <= '9') {
                  int num = 0;
                  for (; *c >= '0' && *c <= '9'; ++c) num = num * 10 + (*c - '0');
                  if (num >= 830 || num >= 740) tier = 3;
                  else if (num >= 720) tier = 2;
                  else tier = 1;
                  break;
              }
          }
          static constexpr uint32_t defaultThresholds[] = {600, 1200, 2000};
          threshold = (tier >= 1 && tier <= 3) ? defaultThresholds[tier - 1] : 600;
      }
  }

  void StarEngine::initializeProfile(uint32_t& threshold, bool& enabled, bool& bindSkip, uint32_t& tier, DxvkDevice* device, bool isD3D9) {
      if (device == nullptr) return;
      initializeProfile(threshold, enabled, bindSkip, tier, device);

      if (isD3D9 && enabled) {
          static constexpr uint32_t d3d9ThresholdTable[] = {1500, 3000, 5000};
          if (tier >= 1 && tier <= 3) {
              threshold = d3d9ThresholdTable[tier - 1];
          } else {
              threshold = 1500;
          }
      }
  }

  // VEGAS: STARENGINE - Governor-style tiered threshold (AdrenoGovernor logic)
  void StarEngine::tuneThreshold(uint32_t& threshold, float load, float frameTime, uint32_t tier) {
      if (tier == 1) {
          if (load > 0.90f && frameTime > 25.0f)
              threshold = 1200;
          else if (load < 0.60f)
              threshold = 8000;
      } else if (tier == 2) {
          if (load > 0.93f && frameTime > 29.0f)
              threshold = 1600;
          else if (load < 0.64f)
              threshold = 8000;
      } else {
          if (load > 0.95f && frameTime > 33.0f)
              threshold = 2000;
          else if (load < 0.70f)
              threshold = 8000;
      }
  }

  // VEGAS: STARENGINE - ZeroInitShaders = 1 (always enable for Unity/Adreno stability)
  bool StarEngine::shouldZeroInit(uint32_t tier) {
      return true;
  }


  void StarEngine::calculateAspectRatio(uint32_t w, uint32_t h, float& outX, float& outY) {
    if (w == 0 || h == 0) { outX = 1.0f; outY = 1.0f; return; }
    constexpr float targetRatio = 16.0f / 9.0f;
    float currentRatio = static_cast<float>(w) / static_cast<float>(h);

    if (currentRatio > targetRatio) {
        outX = targetRatio / currentRatio;
        outY = 1.0f;
    } else if (currentRatio < targetRatio) {
        outX = 1.0f;
        outY = currentRatio / targetRatio;
    } else {
        outX = 1.0f;
        outY = 1.0f;
    }
  }

  uint64_t StarEngine::getSystemRamMB() {
#ifdef _WIN32
      MEMORYSTATUSEX statex;
      statex.dwLength = sizeof(statex);
      GlobalMemoryStatusEx(&statex);
      return statex.ullTotalPhys / (1024 * 1024);
#else
      long pageSize = sysconf(_SC_PAGE_SIZE);
      long pages = sysconf(_SC_PHYS_PAGES);
      if (pageSize <= 0 || pages <= 0) return 4096ULL;
      return static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize) / (1024ULL * 1024ULL);
#endif
  }

  void StarEngine::applyVramSwap(VkPhysicalDeviceMemoryProperties& props, uint32_t tier) {
    uint64_t systemRamBytes = getSystemRamMB() * 1024ULL * 1024ULL;
    if (systemRamBytes == 0) return;

    static constexpr float vramRatioTable[] = { 0.25f, 0.33f, 0.40f };
    float ratio = (tier >= 1 && tier <= 3) ? vramRatioTable[tier - 1] : 0.25f;

    uint64_t extraVram = static_cast<uint64_t>(static_cast<double>(systemRamBytes) * static_cast<double>(ratio));
    extraVram = std::clamp(extraVram, 1ULL << 30, 8ULL << 30);

    uint64_t maxSafeVram = systemRamBytes / 2ULL;

    for (uint32_t i = 0; i < props.memoryHeapCount; i++) {
        if (props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            uint64_t newSize = props.memoryHeaps[i].size + extraVram;
            props.memoryHeaps[i].size = std::min(newSize, maxSafeVram);
        }
    }
  }

  void StarEngine::applyGpuMask(VkPhysicalDeviceProperties& props, uint32_t persona) {
       if (persona < 1 || persona > 3) return;

       struct PersonaConfig {
           uint32_t vendorID;
           uint32_t deviceID;
           const char* deviceName;
       };

       static constexpr PersonaConfig personas[] = {
           {0x10DE, 0x1C82, "NVIDIA GeForce GTX 1050 Ti (StarEngine)"},
           {0x10DE, 0x2184, "NVIDIA GeForce GTX 1660 (StarEngine)"},
           {0x10DE, 0x2520, "NVIDIA GeForce RTX 3060 Laptop GPU (StarEngine)"}
       };

       const auto& config = personas[persona - 1];
       props.vendorID = config.vendorID;
       props.deviceID = config.deviceID;
       std::strncpy(props.deviceName, config.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
       props.deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = '\0';
  }

  // VEGAS: STARENGINE - Enhanced LSFG with tiered frame time thresholds
  bool StarEngine::needsFrameGen(float frameTime, uint32_t tier) {
      if (tier == 1) return false;
      if (tier == 2) return frameTime > 29.0f;
      return frameTime > 33.0f;
  }

  void StarEngine::calculateFsrConstants(StarFsrConstants& c, VkExtent3D src, VkExtent3D dst) {
      if (dst.width == 0 || dst.height == 0) {
          c.info[0] = c.info[1] = 1.0f; c.info[2] = c.info[3] = 0.0f; return;
      }
      c.info[0] = static_cast<float>(src.width)  / static_cast<float>(dst.width);
      c.info[1] = static_cast<float>(src.height) / static_cast<float>(dst.height);
      c.info[2] = 0.5f * c.info[0] - 0.5f;
      c.info[3] = 0.5f * c.info[1] - 0.5f;
  }


  StarPerformanceState StarEngine::analyzePerformance(float load, float frameTime, float targetFrameTime) {
      thread_local float s_prevFrameTime = 16.6f;
      float delta = std::abs(frameTime - s_prevFrameTime);
      s_prevFrameTime = frameTime;

      // Adaptive thresholds relative to target frame time (Feature #8)
      // targetFrameTime = 1000.0 / fpsLimit; default 16.667ms = 60 FPS
      float laggingThreshold   = targetFrameTime * 1.5f;
      float overheatThreshold  = targetFrameTime * 3.0f;

      if (load >= 0.95f && frameTime >= overheatThreshold) {
          return StarPerformanceState::Overheating;
      }
      if (delta > targetFrameTime * 0.75f) {
          return StarPerformanceState::Stuttering;
      }
      if (frameTime >= laggingThreshold) {
          return StarPerformanceState::Lagging;
      }
      return StarPerformanceState::Normal;
  }

  uint32_t StarEngine::getGraphColor(StarPerformanceState state) {
      static constexpr uint32_t colorTable[] = {
          0x00FF00,
          0xFFFF00,
          0xFF8800,
          0xFF0000
      };
      return colorTable[static_cast<int>(state)];
  }


  const char* StarEngine::getStatusString(StarPerformanceState state) {
      static constexpr const char* statusTable[] = {
          "NORMAL",
          "LAGGING",
          "STUTTERING",
          "OVERHEATING"
      };
      return statusTable[static_cast<int>(state)];
  }

  // VEGAS: STARENGINE - ASTC Texture Compression (Feature #9)
  bool StarEngine::formatIsBcn(VkFormat format) {
    switch (format) {
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      case VK_FORMAT_BC2_UNORM_BLOCK:
      case VK_FORMAT_BC2_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC4_UNORM_BLOCK:
      case VK_FORMAT_BC4_SNORM_BLOCK:
      case VK_FORMAT_BC5_UNORM_BLOCK:
      case VK_FORMAT_BC5_SNORM_BLOCK:
      case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      case VK_FORMAT_BC6H_SFLOAT_BLOCK:
      case VK_FORMAT_BC7_UNORM_BLOCK:
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return true;
      default:
        return false;
    }
  }

  VkFormat StarEngine::getAstcFormat(VkFormat bcnFormat) {
    switch (bcnFormat) {
      // BC1 -> ASTC 6x6 (best density, similar bitrate)
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;

      // BC2 -> ASTC 5x5 (better quality at ~same bitrate)
      case VK_FORMAT_BC2_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
      case VK_FORMAT_BC2_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;

      // BC3 -> ASTC 5x5 (1.5x savings, best target)
      case VK_FORMAT_BC3_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
      case VK_FORMAT_BC3_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;

      // BC4 (single channel) -> ASTC 6x6
      case VK_FORMAT_BC4_UNORM_BLOCK:
      case VK_FORMAT_BC4_SNORM_BLOCK:
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;

      // BC5 (normal maps) -> ASTC 5x5
      case VK_FORMAT_BC5_UNORM_BLOCK:
      case VK_FORMAT_BC5_SNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;

      // BC6H (HDR) -> skip, no good ASTC HDR match
      case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        return VK_FORMAT_UNDEFINED;

      // BC7 -> ASTC 5x5 (1.5x savings)
      case VK_FORMAT_BC7_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;

      default:
        return VK_FORMAT_UNDEFINED;
    }
  }

  VkFormat StarEngine::shouldTranscodeFormat(
      VkFormat              originalFormat,
      VkImageUsageFlags     usage,
      VkExtent3D            extent,
      const Rc<DxvkAdapter>& adapter) {
    // Guard: skip render targets, depth/stencil, storage images
    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_STORAGE_BIT))
      return VK_FORMAT_UNDEFINED;

    // Guard: skip small images (UI/icons/fonts < 512 KB)
    uint64_t pixelCount = uint64_t(extent.width) * extent.height * std::max(extent.depth, 1u);
    uint64_t totalBytes = pixelCount * 4; // approximate at 4 bytes/pixel worst case
    if (totalBytes < (512u * 1024u))
      return VK_FORMAT_UNDEFINED;

    // Guard: only compress BCn formats
    if (!formatIsBcn(originalFormat))
      return VK_FORMAT_UNDEFINED;

    // Guard: skip BC6H (HDR) - no good ASTC HDR match
    if (originalFormat == VK_FORMAT_BC6H_UFLOAT_BLOCK ||
        originalFormat == VK_FORMAT_BC6H_SFLOAT_BLOCK)
      return VK_FORMAT_UNDEFINED;

    // Guard: check device supports the target ASTC format
    VkFormat astcFormat = getAstcFormat(originalFormat);
    if (astcFormat == VK_FORMAT_UNDEFINED)
      return VK_FORMAT_UNDEFINED;

    VkFormatFeatureFlags2 features = adapter->getFormatFeatures(astcFormat).optimal;
    if (!(features & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT))
      return VK_FORMAT_UNDEFINED;

    return astcFormat;
  }

  // ============================================================
  // Feature #9: CPU-side BCn→ASTC transcoder
  // ============================================================

  namespace {

    // Interpolate between two RGB565 colors using 2-bit weights
    void decodeBC1(uint8_t dst[16][4], const uint8_t src[8]) {
      uint16_t c0 = src[0] | (uint16_t(src[1]) << 8);
      uint16_t c1 = src[2] | (uint16_t(src[3]) << 8);

      uint8_t r[4], g[4], b[4];
      r[0] = uint8_t((c0 >> 11) * 255 / 31);
      g[0] = uint8_t(((c0 >> 5) & 0x3F) * 255 / 63);
      b[0] = uint8_t((c0 & 0x1F) * 255 / 31);
      r[1] = uint8_t((c1 >> 11) * 255 / 31);
      g[1] = uint8_t(((c1 >> 5) & 0x3F) * 255 / 63);
      b[1] = uint8_t((c1 & 0x1F) * 255 / 31);

      if (c0 > c1) {
        r[2] = (2 * r[0] + r[1]) / 3; g[2] = (2 * g[0] + g[1]) / 3; b[2] = (2 * b[0] + b[1]) / 3;
        r[3] = (r[0] + 2 * r[1]) / 3; g[3] = (g[0] + 2 * g[1]) / 3; b[3] = (b[0] + 2 * b[1]) / 3;
      } else {
        r[2] = (r[0] + r[1]) / 2; g[2] = (g[0] + g[1]) / 2; b[2] = (b[0] + b[1]) / 2;
        r[3] = 0; g[3] = 0; b[3] = 0;
      }

      uint32_t indices = src[4] | (uint32_t(src[5]) << 8) | (uint32_t(src[6]) << 16) | (uint32_t(src[7]) << 24);
      for (int i = 0; i < 16; i++) {
        int idx = (indices >> (2 * i)) & 3;
        dst[i][0] = r[idx]; dst[i][1] = g[idx]; dst[i][2] = b[idx];
        dst[i][3] = (c0 > c1) ? 255u : ((idx == 3) ? 0u : 255u);
      }
    }

    // Decode BC4 alpha block (8 bytes) into 16 alpha values
    void decodeBC4Alpha(uint8_t dst[16], const uint8_t src[8]) {
      uint8_t a0 = src[0], a1 = src[1];
      uint64_t indices = uint64_t(src[2]) | (uint64_t(src[3]) << 8)
                       | (uint64_t(src[4]) << 16) | (uint64_t(src[5]) << 24)
                       | (uint64_t(src[6]) << 32) | (uint64_t(src[7]) << 40);
      for (int i = 0; i < 16; i++) {
        int idx = (indices >> (3 * i)) & 7;
        if (a0 > a1) {
          static const uint8_t bc4[8] = {0,1,2,3,4,5,6,7};
          dst[i] = uint8_t((a0 * (7 - bc4[idx]) + a1 * bc4[idx]) / 7);
        } else {
          if (idx == 0) dst[i] = a0;
          else if (idx == 1) dst[i] = a1;
          else if (idx <= 5) dst[i] = uint8_t(((6 - idx) * a0 + (idx - 1) * a1) / 5);
          else dst[i] = 0;
        }
      }
    }

    // BC4 for single-channel (R)
    void decodeBC4(uint8_t dst[16], const uint8_t src[8]) {
      decodeBC4Alpha(dst, src);
    }

    // Decode BC3 block (16 bytes) to 16 RGBA texels
    void decodeBC3(uint8_t dst[16][4], const uint8_t src[16]) {
      decodeBC1(dst, src);
      uint8_t alpha[16];
      decodeBC4Alpha(alpha, src + 8);
      for (int i = 0; i < 16; i++)
        dst[i][3] = alpha[i];
    }

    // Decode BC5 block (16 bytes) → RG texels
    void decodeBC5(uint8_t dst[16][4], const uint8_t src[16]) {
      uint8_t r[16], g[16];
      decodeBC4(r, src);
      decodeBC4(g, src + 8);
      for (int i = 0; i < 16; i++) {
        dst[i][0] = r[i]; dst[i][1] = g[i];
        dst[i][2] = 0;    dst[i][3] = 255;
      }
    }

    // Decode BC7 block (16 bytes) → RGBA — simplified: use mode 6 only
    void decodeBC7(uint8_t dst[16][4], const uint8_t src[16]) {
      uint8_t mode = src[0];
      for (int i = 0; i < 16; i++) {
        dst[i][0] = 128; dst[i][1] = 128;
        dst[i][2] = 128; dst[i][3] = 255;
      }
      if ((mode & 0x80) == 0) return; // not mode 6 (mode 6 = bit 7 = 1)

      uint16_t r0 = ((src[1] >> 1) & 0x7F) << 1 | (src[2] >> 7);
      uint16_t r1 = (src[2] & 0x7F) << 1 | (src[3] >> 7);
      uint16_t g0 = ((src[3] >> 1) & 0x7F) << 1 | (src[4] >> 7);
      uint16_t g1 = (src[4] & 0x7F) << 1 | (src[5] >> 7);
      uint16_t b0 = ((src[5] >> 1) & 0x7F) << 1 | (src[6] >> 7);
      uint16_t b1 = (src[6] & 0x7F) << 1 | (src[7] >> 7);
      uint8_t a0 = src[8], a1 = src[9];

      uint64_t indices = 0;
      for (int j = 0; j < 6; j++)
        indices |= uint64_t(src[10 + j]) << (8 * j);

      for (int i = 0; i < 16; i++) {
        int idx = (indices >> (4 * i)) & 0xF;
        dst[i][0] = uint8_t(((r0 * (64 - idx) + r1 * idx) * 255) / (63 * 64));
        dst[i][1] = uint8_t(((g0 * (64 - idx) + g1 * idx) * 255) / (63 * 64));
        dst[i][2] = uint8_t(((b0 * (64 - idx) + b1 * idx) * 255) / (63 * 64));
        dst[i][3] = uint8_t(((a0 * (64 - idx) + a1 * idx) * 255) / (63 * 64));
      }
    }

    // --- ASTC block encoder (simplified, 1 partition, LDR) ---

    // Pack multiple bits into an ASTC block
    void astcSetBits(uint8_t* block, uint32_t& bitPos, uint32_t count, uint32_t value) {
      for (uint32_t i = 0; i < count; i++) {
        uint32_t byteIdx = bitPos >> 3;
        uint32_t bitIdx = bitPos & 7;
        if (value & (1u << i))
          block[byteIdx] |= (1u << bitIdx);
        else
          block[byteIdx] &= ~(1u << bitIdx);
        bitPos++;
      }
    }

    // Compute block mode bits for a given grid width, height, and weight bits per texel
    // Implements both direct and base4 modes from the ASTC spec (Table 98).
    uint32_t astcBlockMode(uint32_t w, uint32_t h, uint32_t bits) {
      if (w <= 11 && h <= 5 && bits >= 2 && bits <= 5) {
        // Direct mode: A=0, bit1=0
        uint32_t b = w - 4;  // 0..7
        uint32_t c = h - 2;  // 0..3
        uint32_t d = bits - 2; // 0..3
        return (0 << 0) | (0 << 1) | ((b & 7) << 2) | ((c & 3) << 5) | ((d & 3) << 7);
      }
      // Base4 mode: A=0, bit1=1, with (B,C) = (3,3) for variable-size blocks
      // grid_width = D + 2, grid_height = D + 2 (D = bits[8:6], 3-bit)
      // weight_bits = bits[10:9] + 1
      if (w == h) {
        uint32_t D = w - 2;  // 0..7 for w 2..9
        uint32_t wb = bits - 1; // 0..3 for bits 1..4
        return (0 << 0) | (1 << 1) | (3 << 2) | (3 << 4) | ((D & 7) << 6) | ((wb & 3) << 9);
      }
      // Fallback: treat as square
      return (0 << 0) | (1 << 1) | (3 << 2) | (3 << 4) | (4 << 6) | (1 << 9);
    }

    // Get block dimensions from VkFormat
    bool astcBlockDims(VkFormat fmt, int& bw, int& bh) {
      switch (fmt) {
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:   bw = 4;  bh = 4;  return true;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:   bw = 5;  bh = 4;  return true;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:   bw = 5;  bh = 5;  return true;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:   bw = 6;  bh = 5;  return true;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:   bw = 6;  bh = 6;  return true;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:   bw = 8;  bh = 5;  return true;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:   bw = 8;  bh = 6;  return true;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:   bw = 8;  bh = 8;  return true;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:  bw = 10; bh = 5;  return true;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:  bw = 10; bh = 6;  return true;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:  bw = 10; bh = 8;  return true;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: bw = 10; bh = 10; return true;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: bw = 12; bh = 10; return true;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: bw = 12; bh = 12; return true;
        default: bw = 0; bh = 0; return false;
      }
    }

    // Encode a block of pixels to ASTC
    void encodeAstcBlock(uint8_t* dst, const uint8_t pixels[], int bw, int bh, bool hasAlpha) {
      memset(dst, 0, 16);

      // Determine weight bit count based on block size
      // 4x4: 4 bits (good quality), 5x5: 3 bits, 6x6+: 2 bits
      int weightBits = (bw <= 4 && bh <= 4) ? 4 : (bw <= 5 && bh <= 5) ? 3 : 2;

      uint32_t bp = 0;

      // Block mode
      uint32_t bm = astcBlockMode(bw, bh, weightBits);
      astcSetBits(dst, bp, 13, bm);

      // Partition count - 1 (0 = 1 partition)
      astcSetBits(dst, bp, 2, 0);

      // CEM: 6 (LDR RGB base+scale) or 8 (LDR RGBA base+scale)
      uint32_t cem = hasAlpha ? 8 : 6;
      astcSetBits(dst, bp, 4, cem);

      // Compute min/max RGB and A
      int pixelCount = bw * bh;
      uint8_t minR = 255, minG = 255, minB = 255, maxR = 0, maxG = 0, maxB = 0;
      uint8_t minA = 255, maxA = 0;

      for (int i = 0; i < pixelCount; i++) {
        uint8_t r = pixels[4*i+0], g = pixels[4*i+1], b = pixels[4*i+2], a = pixels[4*i+3];
        if (r < minR) minR = r; if (r > maxR) maxR = r;
        if (g < minG) minG = g; if (g > maxG) maxG = g;
        if (b < minB) minB = b; if (b > maxB) maxB = b;
        if (a < minA) minA = a; if (a > maxA) maxA = a;
      }

      if (maxR == minR && maxG == minG && maxB == minB && (!hasAlpha || maxA == minA)) {
        // Constant-color block: encode as void-extent
        // Use block mode with all weights = 0, endpoints = avg color
        // Re-use the same approach but all weights zero
        maxR = minR; maxG = minG; maxB = minB; maxA = minA;
      }

      // Quantize endpoints: each value reduced to epBits to fit 128-bit budget
      // With alpha: 8 values (R0,G0,B0,R1,G1,B1,A0,A1)
      // Without alpha: 6 values (R0,G0,B0,R1,G1,B1)
      // Total bits = 13(block mode) + 2(partition) + 4(CEM) + epVals*epBits + pixelCount*weightBits <= 128
      int epBits = 5;
      if (!hasAlpha) {
        if (bw <= 4 && bh <= 4)       epBits = 6; // 6*6+16*4+19 = 119
        else if (bw <= 5 && bh <= 5)  epBits = 5; // 6*5+25*3+19 = 124
        else                           epBits = 4; // 6*4+36*2+19 = 91
      } else {
        if (bw <= 4 && bh <= 4)       epBits = 5; // 8*5+16*4+19 = 123
        else if (bw <= 5 && bh <= 5)  epBits = 4; // 8*4+25*3+19 = 126
        else                           epBits = 4; // 8*4+36*2+19 = 99
      }

      // Pack endpoints (value << shift to avoid expensive division per channel)
      int epShift = 8 - epBits;
      uint32_t epR0 = minR >> epShift, epG0 = minG >> epShift, epB0 = minB >> epShift;
      uint32_t epR1 = maxR >> epShift, epG1 = maxG >> epShift, epB1 = maxB >> epShift;

      if (hasAlpha) {
        uint32_t epA0 = minA >> epShift, epA1 = maxA >> epShift;
        // CEM 8 endpoint order: R0,G0,B0,R1,G1,B1 (RGB base+scale) then A0,A1
        astcSetBits(dst, bp, epBits, epR0);
        astcSetBits(dst, bp, epBits, epG0);
        astcSetBits(dst, bp, epBits, epB0);
        astcSetBits(dst, bp, epBits, epR1);
        astcSetBits(dst, bp, epBits, epG1);
        astcSetBits(dst, bp, epBits, epB1);
        astcSetBits(dst, bp, epBits, epA0);
        astcSetBits(dst, bp, epBits, epA1);
      } else {
        astcSetBits(dst, bp, epBits, epR0);
        astcSetBits(dst, bp, epBits, epG0);
        astcSetBits(dst, bp, epBits, epB0);
        astcSetBits(dst, bp, epBits, epR1);
        astcSetBits(dst, bp, epBits, epG1);
        astcSetBits(dst, bp, epBits, epB1);
      }

      // Compute and pack weights in Morton (Z-order) scan per ASTC spec
      // Morton order interleaves bits of x and y to produce a Z-order curve
      for (int ty = 0; ty < bh; ty++) {
        for (int tx = 0; tx < bw; tx++) {
          // Compute Morton index from tx,ty
          // Interleave bits: for up to 4-bit coords (max 12x12)
          int morton = 0;
          for (int b = 0; b < 4; b++) {
            morton |= ((tx >> b) & 1) << (2 * b);
            morton |= ((ty >> b) & 1) << (2 * b + 1);
          }
          int i = morton; // Z-order index

          uint8_t r = pixels[4*i+0], g = pixels[4*i+1], b = pixels[4*i+2];
          uint8_t a = pixels[4*i+3];

          float w = 0.0f;
          if (maxR > minR) w = std::max(w, float(r - minR) / float(maxR - minR));
          if (maxG > minG) w = std::max(w, float(g - minG) / float(maxG - minG));
          if (maxB > minB) w = std::max(w, float(b - minB) / float(maxB - minB));

          if (hasAlpha && maxA > minA)
            w = std::max(w, float(a - minA) / float(maxA - minA));

          int weight = int(w * ((1 << weightBits) - 1) + 0.5f);
          if (weight < 0) weight = 0;
          if (weight >= (1 << weightBits)) weight = (1 << weightBits) - 1;

          astcSetBits(dst, bp, weightBits, weight);
        }
      }

      // Pad remaining bits to 128
      while (bp < 128)
        astcSetBits(dst, bp, 1, 0);
    }

  } // anonymous namespace

  void StarEngine::transcodeImageData(
      void*                 dstData,
      const void*           srcData,
      VkFormat              srcFormat,
      VkFormat              dstFormat,
      uint32_t              width,
      uint32_t              height) {
    int srcBw = 4, srcBh = 4; // BCn always 4x4
    int dstBw, dstBh;
    if (!astcBlockDims(dstFormat, dstBw, dstBh))
      return;

    int srcBlocksX = (int(width) + srcBw - 1) / srcBw;
    int srcBlocksY = (int(height) + srcBh - 1) / srcBh;
    int dstBlocksX = (int(width) + dstBw - 1) / dstBw;
    int dstBlocksY = (int(height) + dstBh - 1) / dstBh;

    int srcBlockSize = 16; // BC3/BC5/BC7 have 16-byte blocks
    if (srcFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK || srcFormat == VK_FORMAT_BC1_RGB_SRGB_BLOCK ||
        srcFormat == VK_FORMAT_BC1_RGBA_UNORM_BLOCK || srcFormat == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
        srcFormat == VK_FORMAT_BC4_UNORM_BLOCK || srcFormat == VK_FORMAT_BC4_SNORM_BLOCK) {
      srcBlockSize = 8;
    }

    // Scratch buffer for decoded source pixels (one source block row × height)
    int scratchTexels = srcBlocksX * srcBw * height;
    uint8_t* decodedPixels = new uint8_t[scratchTexels * 4];
    uint8_t blockPixels[16][4];
    bool hasAlpha = false;

    // Decode all source blocks to RGBA8
    for (int by = 0; by < srcBlocksY; by++) {
      for (int bx = 0; bx < srcBlocksX; bx++) {
        const uint8_t* srcBlock = (const uint8_t*)srcData + (by * srcBlocksX + bx) * srcBlockSize;

        switch (srcFormat) {
          case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
          case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
          case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
          case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            decodeBC1(blockPixels, srcBlock);
            break;
          case VK_FORMAT_BC3_UNORM_BLOCK:
          case VK_FORMAT_BC3_SRGB_BLOCK:
            decodeBC3(blockPixels, srcBlock);
            hasAlpha = true;
            break;
          case VK_FORMAT_BC4_UNORM_BLOCK:
          case VK_FORMAT_BC4_SNORM_BLOCK: {
            uint8_t* rp = (uint8_t*)blockPixels;
            memset(rp, 0, 16 * 4);
            decodeBC4(rp, srcBlock);
            break;
          }
          case VK_FORMAT_BC5_UNORM_BLOCK:
          case VK_FORMAT_BC5_SNORM_BLOCK:
            decodeBC5(blockPixels, srcBlock);
            break;
          case VK_FORMAT_BC7_UNORM_BLOCK:
          case VK_FORMAT_BC7_SRGB_BLOCK:
            decodeBC7(blockPixels, srcBlock);
            hasAlpha = true;
            break;
          default:
            delete[] decodedPixels;
            return;
        }

        // Write decoded pixels to scratch buffer
        for (uint32_t py = 0; py < srcBw && (by * srcBh + py) < height; py++) {
          for (uint32_t px = 0; px < srcBh && (bx * srcBw + px) < width; px++) {
            int gi = int((by * srcBh + py) * width + (bx * srcBw + px));
            int bi = int(py * srcBw + px);
            decodedPixels[gi*4+0] = blockPixels[bi][0];
            decodedPixels[gi*4+1] = blockPixels[bi][1];
            decodedPixels[gi*4+2] = blockPixels[bi][2];
            decodedPixels[gi*4+3] = blockPixels[bi][3];
          }
        }
      }
    }

    // Now encode ASTC blocks from the decoded scratch buffer
    for (int by = 0; by < dstBlocksY; by++) {
      for (int bx = 0; bx < dstBlocksX; bx++) {
        // Gather pixels for this ASTC block
        uint8_t astcPixels[12 * 12 * 4]; // max ASTC block is 12x12
        int idx = 0;
        for (int py = 0; py < dstBh; py++) {
          int sy = by * dstBh + py;
          if (sy >= int(height)) sy = int(height) - 1;
          for (int px = 0; px < dstBw; px++) {
            int sx = bx * dstBw + px;
            if (sx >= int(width)) sx = int(width) - 1;
            int si = sy * int(width) + sx;
            astcPixels[idx*4+0] = decodedPixels[si*4+0];
            astcPixels[idx*4+1] = decodedPixels[si*4+1];
            astcPixels[idx*4+2] = decodedPixels[si*4+2];
            astcPixels[idx*4+3] = decodedPixels[si*4+3];
            idx++;
          }
        }

        uint8_t astcBlock[16];
        encodeAstcBlock(astcBlock, astcPixels, dstBw, dstBh, hasAlpha);

        int dstIdx = by * dstBlocksX + bx;
        memcpy((uint8_t*)dstData + dstIdx * 16, astcBlock, 16);
      }
    }

    delete[] decodedPixels;
  }

  StarFsr::~StarFsr() {
  }

  bool StarFsr::init(const Rc<vk::DeviceFn>& vkd, VkFormat format) {
    if (!createShaderModule(vkd)) return false;
    if (!createDescriptorSetLayout(vkd)) { destroy(vkd); return false; }
    if (!createPipelineLayout(vkd)) { destroy(vkd); return false; }
    if (!createComputePipeline(vkd)) { destroy(vkd); return false; }
    return true;
  }

  void StarFsr::destroy(const Rc<vk::DeviceFn>& vkd) {
    if (m_pipeline) vkd->vkDestroyPipeline(vkd->device(), m_pipeline, nullptr);
    if (m_pipeLayout) vkd->vkDestroyPipelineLayout(vkd->device(), m_pipeLayout, nullptr);
    if (m_descLayout) vkd->vkDestroyDescriptorSetLayout(vkd->device(), m_descLayout, nullptr);
    if (m_shader) vkd->vkDestroyShaderModule(vkd->device(), m_shader, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    m_pipeLayout = VK_NULL_HANDLE;
    m_descLayout = VK_NULL_HANDLE;
    m_shader = VK_NULL_HANDLE;
  }

  bool StarFsr::createShaderModule(const Rc<vk::DeviceFn>& vkd) {
    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = sizeof(dxvk_fsr_easu_code);
    info.pCode = dxvk_fsr_easu_code;
    return vkd->vkCreateShaderModule(vkd->device(), &info, nullptr, &m_shader) == VK_SUCCESS;
  }

  bool StarFsr::createDescriptorSetLayout(const Rc<vk::DeviceFn>& vkd) {
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = 2;
    info.pBindings = bindings;
    return vkd->vkCreateDescriptorSetLayout(vkd->device(), &info, nullptr, &m_descLayout) == VK_SUCCESS;
  }

  bool StarFsr::createPipelineLayout(const Rc<vk::DeviceFn>& vkd) {
    VkPushConstantRange pc = {};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(float) * 4;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1;
    info.pSetLayouts = &m_descLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pc;
    return vkd->vkCreatePipelineLayout(vkd->device(), &info, nullptr, &m_pipeLayout) == VK_SUCCESS;
  }

  bool StarFsr::createComputePipeline(const Rc<vk::DeviceFn>& vkd) {
    VkPipelineShaderStageCreateInfo stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage = stage;
    info.layout = m_pipeLayout;

    return vkd->vkCreateComputePipelines(vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline) == VK_SUCCESS;
  }

  void StarFsr::dispatch(const Rc<vk::DeviceFn>& vkd, VkCommandBuffer cmd, VkDescriptorSet descSet,
                          VkImageView imageView, VkExtent2D extent) const {
    float consts[4] = {
      float(extent.width), float(extent.height),
      1.0f / float(extent.width), 1.0f / float(extent.height)
    };

    vkd->vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(consts), consts);
    vkd->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkd->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &descSet, 0, nullptr);
    vkd->vkCmdDispatch(cmd, (extent.width + 15) / 16, (extent.height + 15) / 16, 1);
  }

} // namespace dxvk
