// asset_manager.h                                                    -*-C++-*-
#ifndef INCLUDED_ENGINE_ASSET_ASSET_MANAGER_H
#define INCLUDED_ENGINE_ASSET_ASSET_MANAGER_H

// renderer
#include <renderer/material.h>
#include <renderer/mesh.h>
#include <renderer/renderer.h>
#include <renderer/texture.h>

// rhi
#include <rhi/vulkan/vk_context.h>

// scene
#include <scene/scene.h>

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace engine::asset {

struct LoadedModel {
    std::vector<std::shared_ptr<renderer::Mesh> > meshes;
    std::vector<renderer::Material*>              materials;
};

class AssetManager {
  private:
    // DATA

    // Reference to the vulkan context for creating GPU buffers.
    rhi::vulkan::VulkanContext& d_context;

    // Command pool for asset upload operations.
    vk::raii::CommandPool d_transferCommandPool;

    // Caches for loaded assets.
    std::unordered_map<std::string, std::unique_ptr<renderer::Texture> >
                                                      d_textures;
    std::vector<std::unique_ptr<renderer::Material> > d_materials;

    std::unordered_map<std::string, LoadedModel> d_models;

    vk::DescriptorSetLayout d_defaultSetLayout;
    renderer::Renderer*     d_renderer;

  public:
    // CREATORS
    explicit AssetManager(rhi::vulkan::VulkanContext& context,
                          vk::DescriptorSetLayout     defaultSetLayout,
                          renderer::Renderer*         renderer);
    ~AssetManager() = default;

    // MANIPULATORS

    // Retrieves a texture from cache or loads it.
    renderer::Texture* loadMaterial(const std::string& filePath);

    // Creates a new material.
    renderer::Material* createMaterial();

    // Loads a mesh and returns a shared pointer to it.
    LoadedModel loadMesh(const std::string& filePath);
};

}  // close engine::asset namespace

#endif  // INCLUDED_ENGINE_ASSET_ASSET_MANAGER_H