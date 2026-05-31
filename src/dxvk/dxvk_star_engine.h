#pragma once
#include <cstdint>
#include <atomic>
#include "dxvk_include.h"

namespace dxvk {
  class DxvkDevice;
  class DxvkAdapter;

  enum class StarPerformanceState {
      Normal = 0,
      Lagging,
      Stuttering,
      Overheating
  };

  struct StarFsrConstants {
      float info[4]; 
  };

  class StarEngine {
  public:
    static void initializeProfile(uint32_t& threshold, bool& enabled, bool& bindSkip, uint32_t& tier, DxvkDevice* device);
    static void initializeProfile(uint32_t& threshold, bool& enabled, bool& bindSkip, uint32_t& tier, DxvkDevice* device, bool isD3D9);
    static bool shouldZeroInit(uint32_t tier);
    static void calculateAspectRatio(uint32_t w, uint32_t h, float& outX, float& outY);
    static uint64_t getSystemRamMB();
    static void applyVramSwap(VkPhysicalDeviceMemoryProperties& props, uint32_t tier);
    static void applyGpuMask(VkPhysicalDeviceProperties& props, uint32_t persona);
    static bool needsFrameGen(float frameTime, uint32_t tier);
    static void calculateFsrConstants(StarFsrConstants& c, VkExtent3D src, VkExtent3D dst);
    static StarPerformanceState analyzePerformance(float load, float frameTime, float targetFrameTime = 16.667f);
    static uint32_t getGraphColor(StarPerformanceState state);
    static const char* getStatusString(StarPerformanceState state);
    static VkFormat shouldTranscodeFormat(
      VkFormat              originalFormat,
      VkImageUsageFlags     usage,
      VkExtent3D            extent,
      uint32_t              mipLevels);
    static bool formatIsBcn(VkFormat format);
    static VkFormat getAstcFormat(VkFormat bcnFormat);
    static void transcodeImageData(
      void*                 dstData,
      const void*           srcData,
      VkFormat              srcFormat,
      VkFormat              dstFormat,
      uint32_t              width,
      uint32_t              height);

    static bool shouldMergeDraws(const DxvkDevice* device);
    static bool shouldCoalesceBarriers(const DxvkDevice* device);
    static float getAdaptiveScale(const DxvkDevice* device, float frameTimeMs);
    static bool shouldTranscodeToAstc(const DxvkDevice* device);
    static uint32_t getStagingRingSizeMb(const DxvkDevice* device);
    static bool shouldOptimizeSubgroup(const DxvkDevice* device);
    static bool shouldEnableQcom(const DxvkDevice* device);

    static std::atomic<float> s_avgFrameTimeMs;
    static std::atomic<float> s_lastFrameTimeMs;
  };

  class StarFsr {
  public:
    StarFsr() = default;
    ~StarFsr();

    StarFsr(const StarFsr&) = delete;
    StarFsr& operator=(const StarFsr&) = delete;

    bool init(const Rc<vk::DeviceFn>& vkd, VkFormat format, bool enableZeroInit = false);
    void destroy(const Rc<vk::DeviceFn>& vkd);

    bool valid() const { return m_pipeline != VK_NULL_HANDLE; }

    VkDescriptorSetLayout descriptorSetLayout() const { return m_descLayout; }
    VkPipelineLayout pipelineLayout() const { return m_pipeLayout; }
    VkPipeline pipeline() const { return m_pipeline; }

    void dispatch(const Rc<vk::DeviceFn>& vkd, VkCommandBuffer cmd, VkDescriptorSet descSet,
                  VkImageView imageView, VkExtent2D extent) const;

  private:
    bool m_zeroInit = false;
    VkShaderModule m_shader = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;

    bool createShaderModule(const Rc<vk::DeviceFn>& vkd);
    bool createDescriptorSetLayout(const Rc<vk::DeviceFn>& vkd);
    bool createPipelineLayout(const Rc<vk::DeviceFn>& vkd);
    bool createComputePipeline(const Rc<vk::DeviceFn>& vkd);
  };

}