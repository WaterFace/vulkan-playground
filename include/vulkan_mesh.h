#pragma once

#include "types.h"
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;

  static VertexInputDescription getVertexDescription();
};

struct Mesh {
  std::vector<Vertex> vertices;

  AllocatedBuffer vertexBuffer;

  bool loadFromObj(const char* filename);
};
