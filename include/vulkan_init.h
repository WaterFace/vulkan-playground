#pragma once

#include "types.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace vkinit {
VkCommandPoolCreateInfo
commandPoolCreateInfo(uint32_t queueFamilyIndex,
                      VkCommandPoolResetFlags flags = 0);

VkCommandBufferAllocateInfo commandBufferAllocateInfo(
    VkCommandPool pool, uint32_t count = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkPipelineShaderStageCreateInfo
pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                              VkShaderModule shaderModule);

VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();

VkPipelineInputAssemblyStateCreateInfo
inputAssemblyCreateInfo(VkPrimitiveTopology topology);

VkPipelineRasterizationStateCreateInfo
rasterizationStateCreateInfo(VkPolygonMode polygonMode);

VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();

VkPipelineColorBlendAttachmentState colorBlendAttachmentState();

VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags,
                                  VkExtent3D extent);

VkImageViewCreateInfo imageviewCreateInfo(VkFormat format, VkImage image,
                                          VkImageAspectFlags aspectFlags);

VkPipelineDepthStencilStateCreateInfo
depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp);

VkRenderPassBeginInfo renderpassBeginInfo(VkRenderPass renderPass,
                                          VkExtent2D extent,
                                          VkFramebuffer framebuffer);

VkDescriptorSetLayoutBinding
descriptorsetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags,
                           uint32_t binding);

VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type,
                                           VkDescriptorSet dstSet,
                                           VkDescriptorBufferInfo *bufferInfo,
                                           uint32_t binding);
}; // namespace vkinit