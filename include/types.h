#pragma once
#include <vulkan/vulkan_core.h>
#include "vk_mem_alloc.h"

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
};

struct AllocatedImage {
  VkImage image;
  VmaAllocation allocation;
};
