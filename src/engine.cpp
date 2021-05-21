#include "engine.h"
#include "vulkan_textures.h"
#include <stdint.h>
#include <string>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/gtx/transform.hpp>

#include "types.h"
#include "vulkan_init.h"

#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

// this is fine, we're not in a header
using namespace std;

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      std::cout << "Detected Vulkan error: " << err << std::endl;              \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

const vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  auto engine = static_cast<Engine *>(glfwGetWindowUserPointer(window));

  if (action == GLFW_PRESS) {
    // std::cout << scancode << std::endl;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }
  float cameraSpeed = 10.0f;
  if (action == GLFW_PRESS) {
    switch (key) {
    case GLFW_KEY_W:
      engine->mainCamera.velocity.z += cameraSpeed;
      break;
    case GLFW_KEY_S:
      engine->mainCamera.velocity.z -= cameraSpeed;
      break;
    case GLFW_KEY_A:
      engine->mainCamera.velocity.x += cameraSpeed;
      break;
    case GLFW_KEY_D:
      engine->mainCamera.velocity.x -= cameraSpeed;
      break;
    }
  } else if (action == GLFW_RELEASE) {
    switch (key) {
    case GLFW_KEY_W:
      engine->mainCamera.velocity.z -= cameraSpeed;
      break;
    case GLFW_KEY_S:
      engine->mainCamera.velocity.z += cameraSpeed;
      break;
    case GLFW_KEY_A:
      engine->mainCamera.velocity.x -= cameraSpeed;
      break;
    case GLFW_KEY_D:
      engine->mainCamera.velocity.x += cameraSpeed;
      break;
    }
  }
}

bool Engine::loadShaderModule(const char *filePath,
                              VkShaderModule *outShaderModule) {
  ifstream file(filePath, ios::ate | ios::binary);
  if (!file.is_open()) {
    return false;
  }

  size_t fileSize = (size_t)file.tellg();

  vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

  file.seekg(0);
  file.read((char *)buffer.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  createInfo.codeSize = buffer.size() * sizeof(uint32_t);
  createInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    return false;
  }

  *outShaderModule = shaderModule;
  return true;
}

void Engine::init() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window = glfwCreateWindow(windowExtent.width, windowExtent.height,
                            "With vkguide.dev", nullptr, nullptr);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetWindowUserPointer(window, this);

  std::cout << "initializing vulkan" << std::endl;
  initVulkan();
  std::cout << "initializing swapchains" << std::endl;
  initSwapchain();
  std::cout << "initializing commands" << std::endl;
  initCommands();
  std::cout << "initializing default renderpass" << std::endl;
  initDefaultRenderpass();
  std::cout << "initializing framebuffers" << std::endl;
  initFramebuffers();
  std::cout << "initializing sync objects" << std::endl;
  initSyncStructures();
  std::cout << "initializing descriptors" << std::endl;
  initDescriptors();
  std::cout << "initializing pipeline" << std::endl;
  initPipelines();
  std::cout << "loading images" << std::endl;
  loadImages();
  std::cout << "loading meshes" << std::endl;
  loadMeshes();
  std::cout << "initializing scene" << std::endl;
  initScene();

  isInitialized = true;
}

void Engine::initVulkan() {
  vkb::InstanceBuilder builder;

  auto instRet = builder.set_app_name("Vulkan Playground")
                     .request_validation_layers(enableValidationLayers)
                     .require_api_version(1, 1, 0)
                     .use_default_debug_messenger()
                     .build();

  vkb::Instance vkbInst = instRet.value();

  instance = vkbInst.instance;
  debugMessenger = vkbInst.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

  vkb::PhysicalDeviceSelector selector{vkbInst};
  vkb::PhysicalDevice vkbPhysicalDevice =
      selector.set_minimum_version(1, 1).set_surface(surface).select().value();

  vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
  vkb::Device vkbDevice = deviceBuilder.build().value();

  device = vkbDevice.device;
  physicalDevice = vkbPhysicalDevice.physical_device;

  graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  graphicsQueueFamily =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = physicalDevice;
  allocatorInfo.device = device;
  allocatorInfo.instance = instance;
  vmaCreateAllocator(&allocatorInfo, &allocator);

  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  std::cout << "The GPU has a minimum buffer alighment of "
            << physicalDeviceProperties.limits.minUniformBufferOffsetAlignment
            << std::endl;
}

void Engine::initSwapchain() {
  vkb::SwapchainBuilder swapchainBuilder{physicalDevice, device, surface};

  vkb::Swapchain vkbSwapchain =
      swapchainBuilder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(windowExtent.width, windowExtent.height)
          .build()
          .value();

  swapchain = vkbSwapchain.swapchain;
  swapchainImages = vkbSwapchain.get_images().value();
  swapchainImageViews = vkbSwapchain.get_image_views().value();
  swapchainImageFormat = vkbSwapchain.image_format;

  mainDeletionQueue.push_function(
      [=]() { vkDestroySwapchainKHR(device, swapchain, nullptr); });

  VkExtent3D depthImageExtent = {windowExtent.width, windowExtent.height, 1};

  depthFormat = VK_FORMAT_D32_SFLOAT;

  VkImageCreateInfo depthInfo = vkinit::imageCreateInfo(
      depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      depthImageExtent);

  VmaAllocationCreateInfo depthAllocInfo{};
  depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  depthAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(allocator, &depthInfo, &depthAllocInfo,
                          &depthImage.image, &depthImage.allocation, nullptr));

  VkImageViewCreateInfo depthViewInfo = vkinit::imageviewCreateInfo(
      depthFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));

  mainDeletionQueue.push_function([=]() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
  });
}

void Engine::initCommands() {
  auto poolInfo = vkinit::commandPoolCreateInfo(
      graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr,
                                 &frames[i].commandPool));

    auto allocInfo =
        vkinit::commandBufferAllocateInfo(frames[i].commandPool, 1);

    VK_CHECK(
        vkAllocateCommandBuffers(device, &allocInfo, &frames[i].commandBuffer));

    mainDeletionQueue.push_function([=]() {
      vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
    });
  }

  auto uploadCommandPoolInfo =
      vkinit::commandPoolCreateInfo(graphicsQueueFamily);
  VK_CHECK(vkCreateCommandPool(device, &uploadCommandPoolInfo, nullptr,
                               &uploadContext.commandPool));
  mainDeletionQueue.push_function([=]() {
    vkDestroyCommandPool(device, uploadContext.commandPool, nullptr);
  });
}

void Engine::initDefaultRenderpass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.flags = 0;
  depthAttachment.format = depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkAttachmentDescription attachments[2] = {
      colorAttachment,
      depthAttachment,
  };

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 2;
  renderPassInfo.pAttachments = &attachments[0];
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

  mainDeletionQueue.push_function(
      [=]() { vkDestroyRenderPass(device, renderPass, nullptr); });
}

void Engine::initFramebuffers() {
  VkFramebufferCreateInfo fbInfo{};
  fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbInfo.pNext = nullptr;
  fbInfo.renderPass = renderPass;
  fbInfo.attachmentCount = 1;
  fbInfo.width = windowExtent.width;
  fbInfo.height = windowExtent.height;
  fbInfo.layers = 1;

  const uint32_t swapchainImageCount = swapchainImages.size();
  framebuffers = vector<VkFramebuffer>(swapchainImageCount);

  for (uint32_t i = 0; i < swapchainImageCount; i++) {
    VkImageView attachments[2];
    attachments[0] = swapchainImageViews[i];
    attachments[1] = depthImageView;

    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = attachments;

    VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]));

    mainDeletionQueue.push_function([=]() {
      vkDestroyFramebuffer(device, framebuffers[i], nullptr);
      vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    });
  }
}

void Engine::initSyncStructures() {
  VkFenceCreateInfo renderFenceInfo = vkinit::fenceCreateInfo();
  renderFenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateFence(device, &renderFenceInfo, nullptr,
                           &frames[i].renderFence));

    mainDeletionQueue.push_function(
        [=]() { vkDestroyFence(device, frames[i].renderFence, nullptr); });

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                               &frames[i].presentSemaphore));
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                               &frames[i].renderSemaphore));

    mainDeletionQueue.push_function([=]() {
      vkDestroySemaphore(device, frames[i].presentSemaphore, nullptr);
      vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
    });
  }

  VkFenceCreateInfo uploadFenceInfo = vkinit::fenceCreateInfo();

  VK_CHECK(vkCreateFence(device, &uploadFenceInfo, nullptr,
                         &uploadContext.uploadFence));
  mainDeletionQueue.push_function(
      [=]() { vkDestroyFence(device, uploadContext.uploadFence, nullptr); });
}

void Engine::initDescriptors() {
  vector<VkDescriptorPoolSize> sizes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
  };

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = 0;
  poolInfo.maxSets = 10;
  poolInfo.poolSizeCount = (uint32_t)sizes.size();
  poolInfo.pPoolSizes = sizes.data();

  vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

  mainDeletionQueue.push_function(
      [=]() { vkDestroyDescriptorPool(device, descriptorPool, nullptr); });

  const size_t sceneParamBufferSize =
      MAX_FRAMES_IN_FLIGHT * padUniformBufferSize(sizeof(GPUSceneData));
  sceneParameterBuffer =
      createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VMA_MEMORY_USAGE_CPU_TO_GPU);

  mainDeletionQueue.push_function([=]() {
    vmaDestroyBuffer(allocator, sceneParameterBuffer.buffer,
                     sceneParameterBuffer.allocation);
  });

  VkDescriptorSetLayoutBinding camBufferBinding =
      vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_SHADER_STAGE_VERTEX_BIT, 0);

  VkDescriptorSetLayoutBinding sceneBinding =
      vkinit::descriptorsetLayoutBinding(
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

  VkDescriptorSetLayoutBinding bindings[] = {camBufferBinding, sceneBinding};

  VkDescriptorSetLayoutCreateInfo setInfo{};
  setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setInfo.pNext = nullptr;

  setInfo.bindingCount = 2;
  setInfo.flags = 0;
  setInfo.pBindings = bindings;

  VK_CHECK(
      vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &globalSetLayout));

  VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorsetLayoutBinding(
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

  VkDescriptorSetLayoutCreateInfo set2Info{};
  set2Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set2Info.bindingCount = 1;
  set2Info.flags = 0;
  set2Info.pNext = nullptr;
  set2Info.pBindings = &objectBind;

  VK_CHECK(vkCreateDescriptorSetLayout(device, &set2Info, nullptr,
                                       &objectSetLayout));

  mainDeletionQueue.push_function([=]() {
    vkDestroyDescriptorSetLayout(device, globalSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);
  });

  cameraBuffer =
      createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VMA_MEMORY_USAGE_CPU_TO_GPU);

  mainDeletionQueue.push_function([=]() {
    vmaDestroyBuffer(allocator, cameraBuffer.buffer, cameraBuffer.allocation);
  });

  VkDescriptorSetAllocateInfo globalAllocInfo{};
  globalAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  globalAllocInfo.pNext = nullptr;
  globalAllocInfo.descriptorPool = descriptorPool;
  globalAllocInfo.descriptorSetCount = 1;
  globalAllocInfo.pSetLayouts = &globalSetLayout;

  vkAllocateDescriptorSets(device, &globalAllocInfo, &globalDescriptor);

  VkDescriptorBufferInfo cameraInfo{};
  cameraInfo.buffer = cameraBuffer.buffer;
  cameraInfo.offset = 0;
  cameraInfo.range = sizeof(GPUCameraData);

  VkDescriptorBufferInfo sceneInfo{};
  sceneInfo.buffer = sceneParameterBuffer.buffer;
  sceneInfo.offset = 0;
  sceneInfo.range = sizeof(GPUSceneData);

  VkWriteDescriptorSet cameraWrite = vkinit::writeDescriptorBuffer(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, globalDescriptor, &cameraInfo, 0);

  VkWriteDescriptorSet sceneWrite =
      vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    globalDescriptor, &sceneInfo, 1);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    const int MAX_OBJECTS = 10000;
    frames[i].objectBuffer = createBuffer(sizeof(GPUObjectData) * MAX_OBJECTS,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                          VMA_MEMORY_USAGE_CPU_TO_GPU);

    mainDeletionQueue.push_function([=]() {
      vmaDestroyBuffer(allocator, frames[i].objectBuffer.buffer,
                       frames[i].objectBuffer.allocation);
    });

    VkDescriptorSetAllocateInfo objectSetAlloc{};
    objectSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objectSetAlloc.pNext = nullptr;
    objectSetAlloc.descriptorPool = descriptorPool;
    objectSetAlloc.descriptorSetCount = 1;
    objectSetAlloc.pSetLayouts = &objectSetLayout;

    vkAllocateDescriptorSets(device, &objectSetAlloc,
                             &frames[i].objectDescriptor);

    VkDescriptorBufferInfo objectBufferInfo{};
    objectBufferInfo.buffer = frames[i].objectBuffer.buffer;
    objectBufferInfo.offset = 0;
    objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

    VkWriteDescriptorSet objectWrite = vkinit::writeDescriptorBuffer(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frames[i].objectDescriptor,
        &objectBufferInfo, 0);

    VkWriteDescriptorSet setWrites[] = {cameraWrite, sceneWrite, objectWrite};

    vkUpdateDescriptorSets(device, 3, setWrites, 0, nullptr);
  }
}

void Engine::initPipelines() {
  VkShaderModule vertShader;
  if (!loadShaderModule("shaders/shader.vert.spv", &vertShader)) {
    cout << "Error when building the vertex shader module" << endl;
  } else {
    cout << "Vertex shader sucessfully loaded" << endl;
  }

  VkShaderModule fragShader;
  if (!loadShaderModule("shaders/shader.frag.spv", &fragShader)) {
    std::cout << "Error when building the fragment shader module" << std::endl;
  } else {
    std::cout << "Fragment shader sucessfully loaded" << std::endl;
  }

  auto pipelineLayoutInfo = vkinit::pipelineLayoutCreateInfo();

  VkPushConstantRange pushConstant;
  pushConstant.offset = 0;
  pushConstant.size = sizeof(MeshPushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
  pipelineLayoutInfo.pushConstantRangeCount = 1;

  VkDescriptorSetLayout setLayouts[] = {globalSetLayout, objectSetLayout};

  pipelineLayoutInfo.setLayoutCount = 2;
  pipelineLayoutInfo.pSetLayouts = setLayouts;

  VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                  &meshPipelineLayout));

  PipelineBuilder pipelineBuilder;

  pipelineBuilder.vertexInputInfo = vkinit::vertexInputStateCreateInfo();
  pipelineBuilder.inputAssembly =
      vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  pipelineBuilder.viewport.x = 0.0f;
  pipelineBuilder.viewport.y = 0.0f;
  pipelineBuilder.viewport.width = (float)windowExtent.width;
  pipelineBuilder.viewport.height = (float)windowExtent.height;
  pipelineBuilder.viewport.minDepth = 0.0f;
  pipelineBuilder.viewport.maxDepth = 1.0f;

  pipelineBuilder.scissor.offset = {0, 0};
  pipelineBuilder.scissor.extent = windowExtent;

  pipelineBuilder.depthStencil =
      vkinit::depthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

  pipelineBuilder.rasterizer =
      vkinit::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
  pipelineBuilder.multisampling = vkinit::multisampleStateCreateInfo();
  pipelineBuilder.colorBlendAttachment = vkinit::colorBlendAttachmentState();
  pipelineBuilder.pipelineLayout = meshPipelineLayout;

  VertexInputDescription vertexDescription = Vertex::getVertexDescription();

  pipelineBuilder.vertexInputInfo.pVertexAttributeDescriptions =
      vertexDescription.attributes.data();
  pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount =
      vertexDescription.attributes.size();

  pipelineBuilder.vertexInputInfo.pVertexBindingDescriptions =
      vertexDescription.bindings.data();
  pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount =
      vertexDescription.bindings.size();

  pipelineBuilder.shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_VERTEX_BIT, vertShader));

  pipelineBuilder.shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT, fragShader));

  meshPipeline = pipelineBuilder.buildPipeline(device, renderPass);

  createMaterial(meshPipeline, meshPipelineLayout, "defaultmesh");

  vkDestroyShaderModule(device, fragShader, nullptr);
  vkDestroyShaderModule(device, vertShader, nullptr);

  mainDeletionQueue.push_function([=]() {
    vkDestroyPipeline(device, meshPipeline, nullptr);

    vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
  });
}

void Engine::initScene() {
  mainCamera.position = {0.f, -6.f, -10.f};
  mainCamera.velocity = {0.0f, 0.0f, 0.0f};

  RenderObject monkey;
  monkey.mesh = getMesh("monkey");
  monkey.material = getMaterial("defaultmesh");
  monkey.model = glm::mat4{1.0f};

  renderables.push_back(monkey);

  for (int x = -20; x <= 20; x++) {
    for (int y = -20; y <= 20; y++) {
      RenderObject tri;
      tri.mesh = getMesh("triangle");
      tri.material = getMaterial("defaultmesh");
      glm::mat4 translation =
          glm::translate(glm::mat4(1.0f), glm::vec3(x, 0, y));
      glm::mat4 scale =
          glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 0.2f, 0.2f));
      tri.model = translation * scale;

      renderables.push_back(tri);
    }
  }
}

void Engine::loadMeshes() {
  Mesh triangleMesh;
  triangleMesh.vertices.resize(3);

  triangleMesh.vertices[0].position = {1.f, 1.f, 0.5f};
  triangleMesh.vertices[1].position = {-1.f, 1.f, 0.5f};
  triangleMesh.vertices[2].position = {0.f, -1.f, 0.5f};

  triangleMesh.vertices[0].color = {0.f, 1.f, 0.0f};
  triangleMesh.vertices[1].color = {0.f, 1.f, 0.0f};
  triangleMesh.vertices[2].color = {0.f, 1.f, 0.0f};

  Mesh monkeyMesh;
  monkeyMesh.loadFromObj("meshes/monkey.obj");

  uploadMesh(triangleMesh);
  uploadMesh(monkeyMesh);

  meshes["triangle"] = triangleMesh;
  meshes["monkey"] = monkeyMesh;
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage,
                                     VmaMemoryUsage memoryUsage) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.pNext = nullptr;

  bufferInfo.size = allocSize;
  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaallocInfo{};
  vmaallocInfo.usage = memoryUsage;

  AllocatedBuffer newBuffer;

  VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo,
                           &newBuffer.buffer, &newBuffer.allocation, nullptr));

  return newBuffer;
}

size_t Engine::padUniformBufferSize(size_t originalSize) {
  size_t minUboAlignment =
      physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
  size_t alignedSize = originalSize;
  if (minUboAlignment > 0) {
    alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
  }
  return alignedSize;
}

FrameData &Engine::getCurrentFrame() {
  return frames[frameNumber % MAX_FRAMES_IN_FLIGHT];
}

void Engine::uploadMesh(Mesh &mesh) {
  const size_t bufferSize = mesh.vertices.size() * sizeof(Vertex);

  VkBufferCreateInfo stagingBufferInfo{};
  stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  stagingBufferInfo.pNext = nullptr;

  stagingBufferInfo.size = bufferSize;
  stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo vmaallocInfo{};
  vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  AllocatedBuffer stagingBuffer;

  VK_CHECK(vmaCreateBuffer(allocator, &stagingBufferInfo, &vmaallocInfo,
                           &stagingBuffer.buffer, &stagingBuffer.allocation,
                           nullptr));

  void *data;
  vmaMapMemory(allocator, stagingBuffer.allocation, &data);
  memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
  vmaUnmapMemory(allocator, stagingBuffer.allocation);

  VkBufferCreateInfo vertexBufferInfo{};
  vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertexBufferInfo.pNext = nullptr;

  vertexBufferInfo.size = bufferSize;
  vertexBufferInfo.usage =
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VK_CHECK(vmaCreateBuffer(allocator, &vertexBufferInfo, &vmaallocInfo,
                           &mesh.vertexBuffer.buffer,
                           &mesh.vertexBuffer.allocation, nullptr));

  immediateSubmit([&](VkCommandBuffer cmd) {
    VkBufferCopy copy;
    copy.dstOffset = 0;
    copy.srcOffset = 0;
    copy.size = bufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1,
                    &copy);
  });

  mainDeletionQueue.push_function([=]() {
    vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer,
                     mesh.vertexBuffer.allocation);
  });
  vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

Material *Engine::createMaterial(VkPipeline pipeline, VkPipelineLayout layout,
                                 const std::string &name) {
  Material mat;
  mat.pipeline = pipeline;
  mat.pipelineLayout = layout;
  materials[name] = mat;

  return &materials[name];
}

Material *Engine::getMaterial(const std::string &name) {
  auto it = materials.find(name);
  if (it == materials.end()) {
    return nullptr;
  } else {
    return &(*it).second;
  }
}

Mesh *Engine::getMesh(const std::string &name) {
  auto it = meshes.find(name);
  if (it == meshes.end()) {
    return nullptr;
  } else {
    return &(*it).second;
  }
}

void Engine::moveCamera(float dt) {
  mainCamera.position += mainCamera.velocity * dt;
}

void Engine::setSceneParameters(const float elapsedTime) {
  const float t = elapsedTime / 2.0f;
  sceneParameters.ambientColor = {sin(t), 0.0f, cos(t), 1.0f};
}

void Engine::run() {
  using namespace chrono;
  typedef std::chrono::duration<float> fsec;

  constexpr float maxDeltaTime = 1 / 10.0f;

  static auto startTime = high_resolution_clock::now();
  auto oldTime = high_resolution_clock::now();

  while (!glfwWindowShouldClose(window)) {
    auto newTime = high_resolution_clock::now();
    float frameTime =
        duration<float, seconds::period>(newTime - oldTime).count();
    oldTime = newTime;
    float dt = std::min(frameTime, maxDeltaTime);

    auto currentTime = high_resolution_clock::now();
    float elapsedTime =
        duration<float, seconds::period>(currentTime - startTime).count();

    std::stringstream title;
    title << "FPS: ";
    title << std::fixed << std::setprecision(2) << elapsedTime;
    glfwSetWindowTitle(window, title.str().data());

    glfwPollEvents();

    moveCamera(dt);
    setSceneParameters(elapsedTime);

    draw();
  }
}

void Engine::cleanup() {
  if (isInitialized) {
    vkDeviceWaitIdle(device);

    mainDeletionQueue.flush();

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
  }
}

void Engine::drawObjects(VkCommandBuffer cmd, RenderObject *first, int count) {
  glm::mat4 view = glm::translate(glm::mat4(1.0f), mainCamera.position);
  glm::mat4 projection = glm::perspective(
      glm::radians(90.0f), windowExtent.width / (float)windowExtent.height,
      0.1f, 200.0f);
  projection[1][1] *= -1;

  GPUCameraData camData;
  camData.projection = projection;
  camData.view = view;
  camData.viewproj = projection * view;

  void *data;
  vmaMapMemory(allocator, cameraBuffer.allocation, &data);
  memcpy(data, &camData, sizeof(GPUCameraData));
  vmaUnmapMemory(allocator, cameraBuffer.allocation);

  void *objectData;
  vmaMapMemory(allocator, getCurrentFrame().objectBuffer.allocation,
               &objectData);
  GPUObjectData *objectSSBO = (GPUObjectData *)objectData;
  for (int i = 0; i < count; i++) {
    RenderObject &object = first[i];
    objectSSBO[i].model = object.model;
  }

  vmaUnmapMemory(allocator, getCurrentFrame().objectBuffer.allocation);

  char *sceneData;
  vmaMapMemory(allocator, sceneParameterBuffer.allocation, (void **)&sceneData);

  int frameIndex = frameNumber % MAX_FRAMES_IN_FLIGHT;

  sceneData += padUniformBufferSize(sizeof(GPUSceneData));

  memcpy(sceneData, &sceneParameters, sizeof(GPUSceneData));

  vmaUnmapMemory(allocator, sceneParameterBuffer.allocation);

  Mesh *lastMesh = nullptr;
  Material *lastMaterial = nullptr;
  for (int i = 0; i < count; i++) {
    RenderObject &object = first[i];

    if (object.material != lastMaterial) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        object.material->pipeline);
      lastMaterial = object.material;

      uint32_t uniformOffset =
          padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              object.material->pipelineLayout, 0, 1,
                              &globalDescriptor, 1, &uniformOffset);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              object.material->pipelineLayout, 1, 1,
                              &getCurrentFrame().objectDescriptor, 0, nullptr);
    }

    glm::mat4 model = object.model;
    glm::mat4 mvp = projection * view * model;

    MeshPushConstants constants;
    constants.renderMatrix = object.model;

    vkCmdPushConstants(cmd, object.material->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &constants);

    if (object.mesh != lastMesh) {
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertexBuffer.buffer,
                             &offset);
      lastMesh = object.mesh;
    }
    vkCmdDraw(cmd, object.mesh->vertices.size(), 1, 0, i);
  }
}

void Engine::draw() {
  VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().renderFence, VK_TRUE,
                           UINT64_MAX));
  VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().renderFence));

  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                 getCurrentFrame().presentSemaphore, nullptr,
                                 &swapchainImageIndex));

  VK_CHECK(vkResetCommandBuffer(getCurrentFrame().commandBuffer, 0));

  VkCommandBufferBeginInfo beginInfo = vkinit::commandBufferBeginInfo(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(getCurrentFrame().commandBuffer, &beginInfo));

  VkClearValue clearValue;
  clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  VkClearValue depthClear;
  depthClear.depthStencil.depth = 1.0f;

  VkRenderPassBeginInfo rpInfo = vkinit::renderpassBeginInfo(
      renderPass, windowExtent, framebuffers[swapchainImageIndex]);

  VkClearValue clearValues[] = {
      clearValue,
      depthClear,
  };
  rpInfo.clearValueCount = 2;
  rpInfo.pClearValues = &clearValues[0];

  vkCmdBeginRenderPass(getCurrentFrame().commandBuffer, &rpInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  drawObjects(getCurrentFrame().commandBuffer, renderables.data(),
              renderables.size());

  vkCmdEndRenderPass(getCurrentFrame().commandBuffer);
  VK_CHECK(vkEndCommandBuffer(getCurrentFrame().commandBuffer));

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = nullptr;

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &waitStage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &getCurrentFrame().presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &getCurrentFrame().renderSemaphore;

  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &getCurrentFrame().commandBuffer;

  VK_CHECK(
      vkQueueSubmit(graphicsQueue, 1, &submit, getCurrentFrame().renderFence));

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;

  presentInfo.pSwapchains = &swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &getCurrentFrame().renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

  frameNumber++;
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkRenderPass pass) {
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;

  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.pNext = nullptr;

  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = nullptr;

  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = pass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.pDepthStencilState = &depthStencil;

  VkPipeline newPipeline;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &newPipeline) != VK_SUCCESS) {
    std::cout << "failed to create pipeline" << std::endl;
    return VK_NULL_HANDLE;
  } else {
    return newPipeline;
  }
}

void Engine::immediateSubmit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
  auto cmdAllocInfo =
      vkinit::commandBufferAllocateInfo(uploadContext.commandPool);

  VkCommandBuffer cmd;
  VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd));

  auto cmdBeginInfo = vkinit::commandBufferBeginInfo(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkSubmitInfo submitInfo = vkinit::submitInfo(&cmd);

  VK_CHECK(
      vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadContext.uploadFence));

  vkWaitForFences(device, 1, &uploadContext.uploadFence, VK_TRUE, UINT64_MAX);
  vkResetFences(device, 1, &uploadContext.uploadFence);

  vkResetCommandPool(device, uploadContext.commandPool, 0);
}

void Engine::loadImages() {
  Texture statue;

  vkutil::loadImageFromFile(*this, "textures/statue.png", statue.image);

  VkImageViewCreateInfo imageInfo = vkinit::imageviewCreateInfo(
      VK_FORMAT_R8G8B8A8_SRGB, statue.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(device, &imageInfo, nullptr, &statue.imageView));

  mainDeletionQueue.push_function([=]() {
    vkDestroyImageView(device, statue.imageView, nullptr);
  });

  loadedTextures["statue_diffuse"] = statue;
}
