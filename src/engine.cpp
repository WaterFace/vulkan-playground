#include "engine.h"
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
#include <iostream>
#include <math.h>
#include <queue>
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
  std::cout << "initializing pipeline" << std::endl;
  initPipelines();
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

  VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

  auto allocInfo = vkinit::commandBufferAllocateInfo(commandPool, 1);

  VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &mainCommandBuffer));

  mainDeletionQueue.push_function(
      [=]() { vkDestroyCommandPool(device, commandPool, nullptr); });
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
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = nullptr;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &renderFence));

  mainDeletionQueue.push_function(
      [=]() { vkDestroyFence(device, renderFence, nullptr); });

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = nullptr;
  semaphoreInfo.flags = 0;

  VK_CHECK(
      vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentSemaphore));
  VK_CHECK(
      vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderSemaphore));

  mainDeletionQueue.push_function([=]() {
    vkDestroySemaphore(device, presentSemaphore, nullptr);
    vkDestroySemaphore(device, renderSemaphore, nullptr);
  });
}

void Engine::initPipelines() {
  VkShaderModule meshVertShader;
  if (!loadShaderModule("shaders/triMesh.vert.spv", &meshVertShader)) {
    cout << "Error when building the mesh vertex shader module" << endl;
  } else {
    cout << "Mesh vertex shader sucessfully loaded" << endl;
  }

  VkShaderModule rgbTriangleFragShader;
  if (!loadShaderModule("shaders/rgb_triangle.frag.spv",
                        &rgbTriangleFragShader)) {
    std::cout << "Failed to load RGB triangle fragment shader" << std::endl;
  } else {
    std::cout << "RGB triangle fragment shader sucessfully loaded" << std::endl;
  }

  auto pipelineLayoutInfo = vkinit::pipelineLayoutCreateInfo();

  VkPushConstantRange pushConstant;
  pushConstant.offset = 0;
  pushConstant.size = sizeof(MeshPushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
  pipelineLayoutInfo.pushConstantRangeCount = 1;

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
      VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

  pipelineBuilder.shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT, rgbTriangleFragShader));

  meshPipeline = pipelineBuilder.buildPipeline(device, renderPass);

  createMaterial(meshPipeline, meshPipelineLayout, "defaultmesh");

  vkDestroyShaderModule(device, rgbTriangleFragShader, nullptr);
  vkDestroyShaderModule(device, meshVertShader, nullptr);

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

void Engine::uploadMesh(Mesh &mesh) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = mesh.vertices.size() * sizeof(Vertex);
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  VmaAllocationCreateInfo vmaallocInfo{};
  vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo,
                           &mesh.vertexBuffer.buffer,
                           &mesh.vertexBuffer.allocation, nullptr));

  mainDeletionQueue.push_function([=]() {
    vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer,
                     mesh.vertexBuffer.allocation);
  });

  void *data;
  vmaMapMemory(allocator, mesh.vertexBuffer.allocation, &data);
  memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
  vmaUnmapMemory(allocator, mesh.vertexBuffer.allocation);
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

void Engine::run() {
  using namespace chrono;

  constexpr float maxDeltaTime = 1/60.0f;

  // static auto startTime = high_resolution_clock::now();
  auto currentTime = high_resolution_clock::now();
  // float elapsedTime = duration<float, seconds::period>(currentTime -
  // startTime).count();

  while (!glfwWindowShouldClose(window)) {
    auto newTime = high_resolution_clock::now();
    float frameTime =
        duration<float, seconds::period>(newTime - currentTime).count();
    currentTime = newTime;
    float dt = std::min(frameTime, maxDeltaTime);

    std::string fpsString = std::to_string(dt);
    glfwSetWindowTitle(window, fpsString.data());


    glfwPollEvents();

    moveCamera(dt);

    draw();
  }
}

void Engine::cleanup() {
  if (isInitialized) {
    vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX);

    mainDeletionQueue.flush();

    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
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

  Mesh *lastMesh = nullptr;
  Material *lastMaterial = nullptr;
  for (int i = 0; i < count; i++) {
    RenderObject &object = first[i];

    if (object.material != lastMaterial) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        object.material->pipeline);
      lastMaterial = object.material;
    }

    glm::mat4 model = object.model;
    glm::mat4 mvp = projection * view * model;

    MeshPushConstants constants;
    constants.renderMatrix = mvp;

    vkCmdPushConstants(cmd, object.material->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &constants);

    if (object.mesh != lastMesh) {
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertexBuffer.buffer,
                             &offset);
      lastMesh = object.mesh;
    }
    vkCmdDraw(cmd, object.mesh->vertices.size(), 1, 0, 0);
  }
}

void Engine::draw() {
  VK_CHECK(vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX));
  VK_CHECK(vkResetFences(device, 1, &renderFence));

  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                 presentSemaphore, nullptr,
                                 &swapchainImageIndex));

  VK_CHECK(vkResetCommandBuffer(mainCommandBuffer, 0));

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.pNext = nullptr;
  beginInfo.pInheritanceInfo = nullptr;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(mainCommandBuffer, &beginInfo));

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

  vkCmdBeginRenderPass(mainCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  drawObjects(mainCommandBuffer, renderables.data(), renderables.size());

  vkCmdEndRenderPass(mainCommandBuffer);
  VK_CHECK(vkEndCommandBuffer(mainCommandBuffer));

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = nullptr;

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &waitStage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &renderSemaphore;

  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &mainCommandBuffer;

  VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, renderFence));

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;

  presentInfo.pSwapchains = &swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &renderSemaphore;
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
