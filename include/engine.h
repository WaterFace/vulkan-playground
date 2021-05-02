#pragma once

#include "types.h"

#include <vector>

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

private:
  void initVulkan();
  void initSwapchain();
};
