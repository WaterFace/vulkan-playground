#pragma once

#include "engine.h"
#include "types.h"

namespace vkutil {
bool loadImageFromFile(Engine &engine, const char *path,
                       AllocatedImage &outImage);
}