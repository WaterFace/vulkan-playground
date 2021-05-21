#pragma once

#include "types.h"

#include "vk_mem_alloc.h"

#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vulkan_mesh.h"

struct Camera {
  glm::vec3 position;
  glm::vec3 velocity;
};

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 viewproj;
};

struct GPUObjectData {
  glm::mat4 model;
};

struct GPUSceneData {
  glm::vec4 fogColor; // w is for exponent
  glm::vec4 fogDistances; // x is for min, y for max, zw unused
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection; // w is for sun power
  glm::vec4 sunlightColor;
};

struct UploadContext {
  VkFence uploadFence;
  VkCommandPool commandPool;
};

struct Texture {
  AllocatedImage image;
  VkImageView imageView;
};

struct FrameData {
  VkSemaphore presentSemaphore, renderSemaphore;
  VkFence renderFence;

  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;

  AllocatedBuffer objectBuffer;
  VkDescriptorSet objectDescriptor;
};

struct Material {
  // these are pointers internally, materials don't
  // carry the entire pipeline with them
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
};

struct RenderObject {
  Mesh *mesh;
  Material *material;
  glm::mat4 model;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 renderMatrix;
};

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()> &&function) {
    deletors.push_back(function);
  }

  void flush() {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)();
    }
    deletors.clear();
  }
};

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class Engine {
public:
  bool isInitialized = false;
  int frameNumber = 0;

  VkExtent2D windowExtent{1024, 768};

  struct GLFWwindow *window{nullptr};

  void init();
  void cleanup();
  void draw();
  void run();

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice physicalDevice;
  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkDevice device;
  VkSurfaceKHR surface;

  VkSwapchainKHR swapchain;
  VkFormat swapchainImageFormat;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;

  VkQueue graphicsQueue;
  uint32_t graphicsQueueFamily;

  VkRenderPass renderPass;
  std::vector<VkFramebuffer> framebuffers;

  VkPipelineLayout meshPipelineLayout;
  VkPipeline meshPipeline;

  DeletionQueue mainDeletionQueue;

  VmaAllocator allocator;

  std::vector<RenderObject> renderables;

  std::unordered_map<std::string, Material> materials;
  std::unordered_map<std::string, Mesh> meshes;

  VkImageView depthImageView;
  AllocatedImage depthImage;
  VkFormat depthFormat;

  FrameData frames[MAX_FRAMES_IN_FLIGHT];

  Camera mainCamera;

  VkDescriptorPool descriptorPool;

  VkDescriptorSetLayout globalSetLayout;
  AllocatedBuffer cameraBuffer;
  VkDescriptorSet globalDescriptor;

  VkDescriptorSetLayout objectSetLayout;

  GPUSceneData sceneParameters;
  AllocatedBuffer sceneParameterBuffer;

  UploadContext uploadContext;

  std::unordered_map<std::string, Texture> loadedTextures;

  AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage,
                               VmaMemoryUsage memoryUsage);

  void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
  void loadImages();

private:
  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initDefaultRenderpass();
  void initFramebuffers();
  void initSyncStructures();
  void initPipelines();
  void initScene();
  void initDescriptors();
  void loadMeshes();

  FrameData &getCurrentFrame();


  size_t padUniformBufferSize(size_t originalSize);

  void uploadMesh(Mesh &mesh);
  Material *createMaterial(VkPipeline pipeline, VkPipelineLayout layout,
                           const std::string &name);
  // these return nullptr if it can't be found
  Material *getMaterial(const std::string &name);
  Mesh *getMesh(const std::string &name);

  void drawObjects(VkCommandBuffer cmd, RenderObject *first, int count);
  bool loadShaderModule(const char *filePath, VkShaderModule *outShaderModule);


  void moveCamera(const float dt);
  void setSceneParameters(const float elapsedTime);
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
