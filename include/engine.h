#pragma once

#include "types.h"

#include "vk_mem_alloc.h"

#include <vector>
#include <functional>
#include <deque>
#include <unordered_map>
#include <string>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "vulkan_mesh.h"

struct Camera {
  glm::vec3 position;
  glm::vec3 velocity;
};

struct Material {
  // these are pointers internally, materials don't
  // carry the entire pipeline with them
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
};

struct RenderObject {
  Mesh* mesh;
  Material* material;
  glm::mat4 model;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 renderMatrix;
};

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

  VkPipelineLayout meshPipelineLayout;
  VkPipeline meshPipeline;

  int selectedShader{ 0 };

  DeletionQueue mainDeletionQueue;

  VmaAllocator allocator;

  std::vector<RenderObject> renderables;

  std::unordered_map<std::string, Material> materials;
  std::unordered_map<std::string, Mesh> meshes;

  VkImageView depthImageView;
  AllocatedImage depthImage;
  VkFormat depthFormat;

  Camera mainCamera;

private:
  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderpass();
  void initFramebuffers();
  void initSyncStructures();
  void initPipelines();
  void initScene();
  void loadMeshes();
  void uploadMesh(Mesh& mesh);
  Material* createMaterial(
    VkPipeline pipeline,
    VkPipelineLayout layout,
    const std::string& name
    );
  // these return nullptr if it can't be found
  Material* getMaterial(const std::string& name);
  Mesh* getMesh(const std::string& name);

  void drawObjects(VkCommandBuffer cmd, RenderObject* first, int count);
  bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

  void moveCamera(float dt);
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
  VkPipelineDepthStencilStateCreateInfo depthStencil;

  VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
};
