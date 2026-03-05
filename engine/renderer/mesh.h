// mesg.hpp                                                           -*-C++-*-
#ifndef GAMEGINE_CORE_MESH
#define GAMEGINE_CORE_MESH

//@PURPOSE: Provide a simple mesh wrapper (vertex/index buffers + draw).
//
//@CLASSES:
//  core::VulkanMesh: RAII mechanism for GPU vertex/index buffers.
//
//@DESCRIPTION: This component owns a vertex buffer and an index buffer and
// exposes a `draw(vk::CommandBuffer)` convenience to bind them and call
// `drawIndexed`. It uses a staging upload on construction.

// core
#include <core/vertex.h>

// renderer
#include <renderer/command_pool.h>

// std
#include <string>
#include <vector>

// vulkan
#include <vulkan/vulkan_raii.hpp>

namespace engine::renderer {

// ----------------
// class VulkanMesh
// ----------------
class Mesh {
  private:
    // DATA
    rhi::vulkan::VulkanContext& d_context;

    vk::raii::Buffer       m_vertexBuffer       = nullptr;
    vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;

    vk::raii::Buffer       m_indexBuffer       = nullptr;
    vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

    uint32_t m_indexCount = 0;

  public:
    // CREATORS

    // Constructeur pour créer depuis des vecteurs (pour formes simples)
    Mesh(rhi::vulkan::VulkanContext&      context,
         vk::CommandPool                  commandPool,
         const std::vector<core::Vertex>& vertices,
         const std::vector<uint32_t>&     indices);

    // ACCESSORS
    uint32_t getIndexCount() const { return m_indexCount; }

    void draw(vk::CommandBuffer cmd) const;
};
}  // close core namespace

#endif  // GAMEGINE_CORE_MESH
