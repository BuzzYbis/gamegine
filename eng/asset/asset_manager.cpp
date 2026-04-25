// asset_manager.cpp                                                  -*-C++-*-
#include <asset/asset_manager.h>

// std
#include <filesystem>
#include <iostream>

// asset
#include <asset/asset_modeldata.h>
#include <asset/impt/impt_obj.h>

namespace eng::asset {

AssetManager::AssetManager(rhi::ContextProtocol* context,
                           rnd::Renderer*        renderer)
: d_context_p(context)
, d_renderer_p(renderer)
{
    rhi::ResourceLayoutConfig layoutConfig{};
    layoutConfig.bindings.push_back(
        {0, rhi::ResourceType::TextureSampler, rhi::ShaderStage::Fragment});
    d_materialLayout = d_context_p->createResourceLayout(layoutConfig);
}

rnd::Texture* AssetManager::loadMaterial(const std::string& filePath)
{
    if (d_textures.contains(filePath)) {
        return d_textures[filePath].get();
    }

    auto texture = std::make_unique<rnd::Texture>(d_context_p, filePath);

    rnd::Texture* texturePtr = texture.get();
    d_textures[filePath]     = std::move(texture);

    return texturePtr;
}

rnd::Material* AssetManager::createMaterial()
{
    auto           material    = std::make_unique<rnd::Material>();
    rnd::Material* materialPtr = material.get();

    materialPtr->setPipeline(d_renderer_p->getPipeline("PBR_Opaque"));
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

    std::shared_ptr<impt::ModelImporter> importer;

    if (extension == ".obj") {
        importer = std::make_shared<impt::ObjImporter>();
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

    std::vector<rnd::Material*> uniqueMaterials;
    for (const auto& [name, diffuseTexturePath] : data->materials) {
        rnd::Material* mat = createMaterial();
        rnd::Texture*  tex = nullptr;

        if (!diffuseTexturePath.empty()) {
            tex = loadMaterial(diffuseTexturePath);
        }
        else {
            tex = loadMaterial("textures/no_texture.png");
        }

        mat->setTexture(tex);

        auto resourceSet = d_context_p->createResourceSet(
            d_renderer_p->materialLayout());

        resourceSet->updateTexture(0, tex->protocol());
        mat->setResourceSet(resourceSet.get());
        d_resourceSets.push_back(std::move(resourceSet));

        uniqueMaterials.push_back(mat);
    }

    for (const auto& [vertices, indices, materialIndex] : data->meshes) {
        auto mesh = std::make_shared<rnd::Mesh>(d_context_p,
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

}  // close package namespace