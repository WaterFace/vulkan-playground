#pragma once

#include "types.h"

class Camera {
  glm::vec3 up;
  glm::vec3 forward;

  glm::vec3 position;
  glm::vec3 velocity;

  float speed = 1.0f;
};
