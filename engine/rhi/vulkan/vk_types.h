// vk_types.h                                                         -*-C++-*-
#ifndef INCLUDED_ENGINE_RHI_VULKAN_VK_TYPES_H
#define INCLUDED_ENGINE_RHI_VULKAN_VK_TYPES_H

#include <glm/glm.hpp>

struct MeshPushConstants {
    glm::mat4 renderMatrix;  // Model matrix (offset 0 for shader)
};

#endif  // INCLUDED_ENGINE_RHI_VULKAN_VK_TYPES_H
