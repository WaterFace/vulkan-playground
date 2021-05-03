#include "engine.h"
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.h"
#include "vulkan_init.h"

#include "VkBootstrap.h"

#include <iostream>
#include <vector>
#include <queue>
#include <chrono>
#include <math.h>
#include <fstream>

// this is fine, we're not in a header
using namespace std;

#define VK_CHECK(x)\
do {\
  VkResult err = x;\
  if (err) {\
    std::cout << "Detected Vulkan error: " << err << std::endl; \
    abort();\
  }\
} while(0)

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation",
};

const vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));

  if (action == GLFW_PRESS) {
    // std::cout << scancode << std::endl;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    engine->selectedShader++;
    if (engine->selectedShader > 1) {
      engine->selectedShader = 0;
    }
    std::cout << engine->selectedShader << std::endl;
  }
}

bool Engine::loadShaderModule(const char* filePath, VkShaderModule* outShaderModule) {
  ifstream file(filePath, ios::ate | ios::binary);
  if (!file.is_open()) {
    return false;
  }

  size_t fileSize = (size_t)file.tellg();

  vector<uint32_t> buffer (fileSize / sizeof(uint32_t));

  file.seekg(0);
  file.read((char*)buffer.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  createInfo.codeSize = buffer.size() * sizeof(uint32_t);
  createInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    return false;
  }

  *outShaderModule = shaderModule;
  return true;
}

void Engine::init() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window = glfwCreateWindow(
    windowExtent.width,
    windowExtent.height,
    "With vkguide.dev",
    nullptr,
    nullptr
    );
  glfwSetKeyCallback(window, keyCallback);
  glfwSetWindowUserPointer(window, this);

  initVulkan();
  initSwapchain();
  initCommands();
  initDefaultRenderpass();
  initFramebuffers();
  initSyncStructures();
  initPipelines();

  isInitialized = true;
}

void Engine::initVulkan() {
  vkb::InstanceBuilder builder;

  auto instRet = builder
  .set_app_name("Vulkan Playground")
  .request_validation_layers(enableValidationLayers)
  .require_api_version(1, 1, 0)
  .use_default_debug_messenger()
  .build();

  vkb::Instance vkbInst = instRet.value();

  instance = vkbInst.instance;
  debugMessenger = vkbInst.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

  vkb::PhysicalDeviceSelector selector{ vkbInst };
  vkb::PhysicalDevice vkbPhysicalDevice = selector
  .set_minimum_version(1, 1)
  .set_surface(surface)
  .select()
  .value();

  vkb::DeviceBuilder deviceBuilder { vkbPhysicalDevice };
  vkb::Device vkbDevice = deviceBuilder.build().value();

  device = vkbDevice.device;
  physicalDevice = vkbPhysicalDevice.physical_device;

  graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void Engine::initSwapchain() {
  vkb::SwapchainBuilder swapchainBuilder{ physicalDevice, device, surface };

  vkb::Swapchain vkbSwapchain = swapchainBuilder
  .use_default_format_selection()
  .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
  .set_desired_extent(windowExtent.width, windowExtent.height)
  .build()
  .value();

  swapchain = vkbSwapchain.swapchain;
  swapchainImages = vkbSwapchain.get_images().value();
  swapchainImageViews = vkbSwapchain.get_image_views().value();
  swapchainImageFormat = vkbSwapchain.image_format;

  mainDeletionQueue.push_function([=]() {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  });
}

void Engine::initCommands() {
  auto poolInfo = vkinit::commandPoolCreateInfo(
    graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

  auto allocInfo = vkinit::commandBufferAllocateInfo(commandPool, 1);

  VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &mainCommandBuffer));

  mainDeletionQueue.push_function([=](){
    vkDestroyCommandPool(device, commandPool, nullptr);
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

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

  mainDeletionQueue.push_function([=]() {
    vkDestroyRenderPass(device, renderPass, nullptr);
  });
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
    fbInfo.pAttachments = &swapchainImageViews[i];
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

  mainDeletionQueue.push_function([=]() {
    vkDestroyFence(device, renderFence, nullptr);
  });

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = nullptr;
  semaphoreInfo.flags = 0;

  VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentSemaphore));
  VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderSemaphore));

  mainDeletionQueue.push_function([=]() {
    vkDestroySemaphore(device, presentSemaphore, nullptr);
    vkDestroySemaphore(device, renderSemaphore, nullptr);
  });
}

void Engine::initPipelines() {
  VkShaderModule triangleFragShader;
  if (!loadShaderModule("shaders/triangle.frag.spv", &triangleFragShader)) {
    std::cout << "Failed to load triangle fragment shader" << std::endl;
  } else {
    std::cout << "Triangle fragment shader sucessfully loaded" << std::endl;
  }

  VkShaderModule triangleVertShader;
  if (!loadShaderModule("shaders/triangle.vert.spv", &triangleVertShader)) {
    std::cout << "Failed to load triangle vertex shader" << std::endl;
  } else {
    std::cout << "Triangle vertex shader sucessfully loaded" << std::endl;
  }

  VkShaderModule rgbTriangleFragShader;
  if (!loadShaderModule("shaders/rgb_triangle.frag.spv", &rgbTriangleFragShader)) {
    std::cout << "Failed to load RGB triangle fragment shader" << std::endl;
  } else {
    std::cout << "RGB triangle fragment shader sucessfully loaded" << std::endl;
  }

  VkShaderModule rgbTriangleVertShader;
  if (!loadShaderModule("shaders/rgb_triangle.vert.spv", &rgbTriangleVertShader)) {
    std::cout << "Failed to load RGB triangle vertex shader" << std::endl;
  } else {
    std::cout << "RGB triangle vertex shader sucessfully loaded" << std::endl;
  }

  auto pipelineLayoutInfo = vkinit::pipelineLayoutCreateInfo();
  VK_CHECK(vkCreatePipelineLayout(
    device,
    &pipelineLayoutInfo,
    nullptr,
    &pipelineLayout
    ));

  PipelineBuilder pipelineBuilder;

  pipelineBuilder.shaderStages.push_back(
    vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_VERTEX_BIT,
      triangleVertShader)
    );
  pipelineBuilder.shaderStages.push_back(
    vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      triangleFragShader)
    );

  pipelineBuilder.vertexInputInfo = vkinit::vertexInputStateCreateInfo();
  pipelineBuilder.inputAssembly = vkinit::inputAssemblyCreateInfo(
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    );

  pipelineBuilder.viewport.x = 0.0f;
  pipelineBuilder.viewport.y = 0.0f;
  pipelineBuilder.viewport.width = (float)windowExtent.width;
  pipelineBuilder.viewport.height = (float)windowExtent.height;
  pipelineBuilder.viewport.minDepth = 0.0f;
  pipelineBuilder.viewport.maxDepth = 1.0f;

  pipelineBuilder.scissor.offset = { 0, 0 };
  pipelineBuilder.scissor.extent = windowExtent;

  pipelineBuilder.rasterizer = vkinit::rasterizationStateCreateInfo(
    VK_POLYGON_MODE_FILL
    );
  pipelineBuilder.multisampling = vkinit::multisampleStateCreateInfo();
  pipelineBuilder.colorBlendAttachment = vkinit::colorBlendAttachmentState();
  pipelineBuilder.pipelineLayout = pipelineLayout;

  trianglePipeline = pipelineBuilder.buildPipeline(device, renderPass);

  pipelineBuilder.shaderStages.clear();

  pipelineBuilder.shaderStages.push_back(
    vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_VERTEX_BIT,
      rgbTriangleVertShader)
    );
  pipelineBuilder.shaderStages.push_back(
    vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      rgbTriangleFragShader)
    );

  rgbTrianglePipeline = pipelineBuilder.buildPipeline(device, renderPass);

  vkDestroyShaderModule(device, rgbTriangleVertShader, nullptr);
  vkDestroyShaderModule(device, rgbTriangleFragShader, nullptr);
  vkDestroyShaderModule(device, triangleVertShader, nullptr);
  vkDestroyShaderModule(device, triangleFragShader, nullptr);

  mainDeletionQueue.push_function([=]() {
    vkDestroyPipeline(device, trianglePipeline, nullptr);
    vkDestroyPipeline(device, rgbTrianglePipeline, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  });
}

void Engine::run() {
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    draw();
  }
}

void Engine::cleanup() {
  if (isInitialized) {
    vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX);

    mainDeletionQueue.flush();

    vkDestroySurfaceKHR(instance, surface, nullptr);

vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
  }
}

void Engine::draw() {
  VK_CHECK(vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX));
  VK_CHECK(vkResetFences(device, 1, &renderFence));

  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(
    device,
    swapchain,
    UINT64_MAX,
    presentSemaphore,
    nullptr,
    &swapchainImageIndex));

  VK_CHECK(vkResetCommandBuffer(mainCommandBuffer, 0));

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.pNext = nullptr;
  beginInfo.pInheritanceInfo = nullptr;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(mainCommandBuffer, &beginInfo));

  using namespace chrono;

  static auto startTime = high_resolution_clock::now();
  auto currentTime = high_resolution_clock::now();
  float t = duration<float, seconds::period>(currentTime - startTime).count();
  VkClearValue clearValue;
  float flash = abs(sin(t));
  clearValue.color = { {0.0f, 0.0f, flash, 1.0f} };

  VkRenderPassBeginInfo rpInfo{};
  rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpInfo.pNext = nullptr;
  rpInfo.renderPass = renderPass;
  rpInfo.renderArea.offset.x = 0;
  rpInfo.renderArea.offset.y = 0;
  rpInfo.renderArea.extent = windowExtent;
  rpInfo.framebuffer = framebuffers[swapchainImageIndex];
  rpInfo.clearValueCount = 1;
  rpInfo.pClearValues = &clearValue;

  vkCmdBeginRenderPass(mainCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  if (selectedShader == 0) {
    vkCmdBindPipeline(mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rgbTrianglePipeline);
  } else {
    vkCmdBindPipeline(mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
  }

  vkCmdDraw(mainCommandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(mainCommandBuffer);
  VK_CHECK(vkEndCommandBuffer(mainCommandBuffer));

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = nullptr;

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
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

  VkPipeline newPipeline;
  if (vkCreateGraphicsPipelines(
    device,
    VK_NULL_HANDLE,
    1,
    &pipelineInfo,
    nullptr,
    &newPipeline) != VK_SUCCESS)
  {
    std::cout << "failed to create pipeline" << std::endl;
    return VK_NULL_HANDLE;
  } else
  {
    return newPipeline;
  }
}
