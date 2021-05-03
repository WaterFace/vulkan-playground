#pragma once

#include "types.h"

#include <vector>
#include <functional>
#include <deque>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()>&& function) {
    deletors.push_back(function);
  }

  void flush() {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)();
    }
    deletors.clear();
  }
};

class Engine {
public:
  bool isInitialized = false;
  int frameNumber = 0;

  VkExtent2D windowExtent{ 1024, 768 };

  struct GLFWwindow* window{ nullptr };

  void init();
  void cleanup();
  void draw();
  void run();

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  VkSurfaceKHR surface;

  VkSwapchainKHR swapchain;
  VkFormat swapchainImageFormat;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;

  VkQueue graphicsQueue;
  uint32_t graphicsQueueFamily;

  VkCommandPool commandPool;
  VkCommandBuffer mainCommandBuffer;

  VkRenderPass renderPass;
  std::vector<VkFramebuffer> framebuffers;

  VkSemaphore presentSemaphore, renderSemaphore;
  VkFence renderFence;

  VkPipelineLayout pipelineLayout;
  VkPipeline trianglePipeline;
  VkPipeline rgbTrianglePipeline;

  int selectedShader{ 0 };

  DeletionQueue mainDeletionQueue;

private:
  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderpass();
  void initFramebuffers();
  void initSyncStructures();
  void initPipelines();

  bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule);
};

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  VkPipelineVertexInputStateCreateInfo vertexInputInfo;
  VkPipelineInputAssemblyStateCreateInfo inputAssembly;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineColorBlendAttachmentState colorBlendAttachment;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineLayout pipelineLayout;

  VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
};
