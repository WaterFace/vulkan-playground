#pragma once

#include "types.h"

namespace vkinit {
  VkCommandPoolCreateInfo commandPoolCreateInfo(
    uint32_t queueFamilyIndex,
    VkCommandPoolResetFlags flags = 0
    );
  
  VkCommandBufferAllocateInfo commandBufferAllocateInfo(
    VkCommandPool pool,
    uint32_t count = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    );

  VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(
    VkShaderStageFlagBits stage,
    VkShaderModule shaderModule
    );

  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(
    VkPrimitiveTopology topology
    );

  VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(
    VkPolygonMode polygonMode
    );

  VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();

  VkPipelineColorBlendAttachmentState colorBlendAttachmentState();

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();
};