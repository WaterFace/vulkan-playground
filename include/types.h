#pragma once
#include <vulkan/vulkan_core.h>
#include "vk_mem_alloc.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
};

struct AllocatedImage {
  VkImage image;
  VmaAllocation allocation;
};
