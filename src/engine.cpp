#include "engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.h"
#include "vulkan_init.h"

#include "VkBootstrap.h"

#include <iostream>
#include <vector>
#include <queue>

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
  if (action == GLFW_PRESS) {
    std::cout << scancode << std::endl;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }
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

  initVulkan();

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
}

void Engine::run() {
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    draw();
  }
}

void Engine::cleanup() {
  if (isInitialized) {
    glfwDestroyWindow(window);
  }
}

void Engine::draw() {

}