// asset_manager.cpp                                                  -*-C++-*-
#include <asset/asset_manager.h>

// asset
#include <asset/importers/obj_importer.h>
#include <asset/model_data.h>

// std
#include "core/log.h"

#include <filesystem>
#include <iostream>

namespace engine::asset {
AssetManager::AssetManager(rhi::vulkan::VulkanContext&   context,
                           const vk::Format              defaultColorFormat,
                           const vk::Format              defaultDepthFormat,
                           const vk::DescriptorSetLayout defaultSetLayout)
: d_context(context)
, d_defaultColorFormat(defaultColorFormat)
, d_defaultDepthFormat(defaultDepthFormat)
, d_defaultSetLayout(defaultSetLayout)
, d_transferCommandPool(nullptr)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;

    poolInfo.queueFamilyIndex = d_context.graphicsQueueFamilyIndex();

    d_transferCommandPool = vk::raii::CommandPool(d_context.device(),
                                                  poolInfo);
}

renderer::Texture* AssetManager::loadMaterial(const std::string& filePath)
{
    // Check if we already load the texture
    if (d_textures.contains(filePath)) {
        return d_textures[filePath].get();
    }

    // If the cache doesn't have it, we create a new mesh and store it there
    auto               texture = std::make_unique<renderer::Texture>(d_context,
                                                       d_transferCommandPool,
                                                       filePath);
    renderer::Texture* texturePtr = texture.get();
    d_textures[filePath]          = std::move(texture);

    return texturePtr;
}

renderer::Material*
AssetManager::createMaterial(vk::Format              colorFormat,
                             vk::Format              depthFormat,
                             vk::DescriptorSetLayout setLayout)
{
    auto material = std::make_unique<renderer::Material>(d_context,
                                                         colorFormat,
                                                         depthFormat,
                                                         setLayout);

    renderer::Material* materialPtr = material.get();
    d_materials.push_back(std::move(material));

    return materialPtr;
}

LoadedModel AssetManager::loadMesh(const std::string& filePath)
{
    const auto it = d_models.find(filePath);
    if (it != d_models.end()) {
        std::cout << "[AssetManager] Returning cached mesh: " << filePath
                  << "\n";
        return it->second;
    }

    std::cout << "[AssetManager] Loading new mesh from disk: " << filePath
              << "\n";

    const std::filesystem::path path(filePath);
    const std::string           extension = path.extension().string();

    std::shared_ptr<ModelImporter> importer;

    if (extension == ".obj") {
        importer = std::make_shared<ObjImporter>();
    }
    else {
        std::cerr << "[AssetManager] Unsupported mesh format: " << extension
                  << "\n";
        return {};
    }

    const std::shared_ptr<ModelData> data = importer->load(filePath);

    if (!data || data->meshes.empty()) {
        std::cerr << "[AssetManager] Failed to load or empty mesh data: "
                  << filePath << "\n";
        return {};
    }

    LoadedModel result;

    if (data->materials.empty()) {
        MaterialData fallbackMat;
        fallbackMat.name = "default_fallback";
        data->materials.push_back(fallbackMat);
    }

    std::vector<renderer::Material*> uniqueMaterials;
    for (const auto& [name, diffuseTexturePath] : data->materials) {
        renderer::Material* mat = createMaterial(d_defaultColorFormat,
                                                 d_defaultDepthFormat,
                                                 d_defaultSetLayout);

        renderer::Texture* tex = nullptr;
        if (!diffuseTexturePath.empty()) {
            tex = loadMaterial(diffuseTexturePath);
        }
        else {
            tex = loadMaterial("textures/viking_room.png");
        }

        mat->setTexture(tex);

        uniqueMaterials.push_back(mat);
    }

    for (const auto& [vertices, indices, materialIndex] : data->meshes) {
        auto mesh = std::make_shared<renderer::Mesh>(d_context,
                                                     *d_transferCommandPool,
                                                     vertices,
                                                     indices);
        result.meshes.push_back(mesh);

        int safeMatIndex = materialIndex;
        if (safeMatIndex < 0) {
            safeMatIndex = 0;
        }

        if (safeMatIndex < uniqueMaterials.size()) {
            result.materials.push_back(uniqueMaterials[safeMatIndex]);
        }
        else {
            result.materials.push_back(uniqueMaterials.front());
        }
    }

    d_models[filePath] = result;

    return result;
}

}  // close engine::asset namespace