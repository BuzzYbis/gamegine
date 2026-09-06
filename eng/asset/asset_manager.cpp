// asset_manager.cpp                                                  -*-C++-*-
#include <asset/asset_manager.h>

// std
#include <array>
#include <filesystem>
#include <iostream>
#include <string>

// asset
#include <asset/asset_modeldata.h>
#include <asset/impt/impt_gltf.h>
#include <asset/impt/impt_obj.h>

namespace eng::asset {
namespace {

/// Return a key identifying, in the texture cache, the image of the
/// specified 'name' uploaded with the specified 'format'.
std::string makeTextureKey(const std::string& name, const rhi::Format format)
{
    return name + '|' + std::to_string(static_cast<int>(format));
}

/// Return the alpha mode the renderer is to draw the specified
/// 'materialData' with. The two enumerations are declared apart because the
/// renderer sits below this package and cannot depend on it.
///
/// A transmissive material is composited whatever alpha mode it declares:
/// the renderer holds no copy of what stands behind a surface and cannot
/// refract it, so it approximates transmission by blending, which is the
/// closest of the modes it does implement.
rnd::AlphaMode toRenderAlphaMode(const MaterialData& materialData)
{
    if (materialData.transmissionFactor > 0.0f) {
        return rnd::AlphaMode::Blend;
    }

    switch (materialData.alphaMode) {
    case AlphaMode::Mask: return rnd::AlphaMode::Mask;
    case AlphaMode::Blend: return rnd::AlphaMode::Blend;
    case AlphaMode::Opaque: break;
    }

    return rnd::AlphaMode::Opaque;
}

/// Return the factors of the specified 'materialData', in the layout the
/// shader expects them.
rnd::MaterialParams toMaterialParams(const MaterialData& materialData)
{
    rnd::MaterialParams params;

    params.baseColorFactor    = materialData.baseColorFactor;
    params.emissiveFactor     = materialData.emissiveFactor;
    params.metallicFactor     = materialData.metallicFactor;
    params.roughnessFactor    = materialData.roughnessFactor;
    params.normalScale        = materialData.normalScale;
    params.occlusionStrength  = materialData.occlusionStrength;
    params.transmissionFactor = materialData.transmissionFactor;

    // Only a masked material discards. An opaque one ignores its alpha
    // altogether and a blended one composites it, so both are given a cutoff
    // no fragment can fall below rather than a branch in the shader.
    params.alphaCutoff = materialData.alphaMode == AlphaMode::Mask
                             ? materialData.alphaCutoff
                             : 0.0f;

    return params;
}

/// Return the index, in the image collection of a model, of the texture the
/// specified 'materialData' declares for the specified 'slot', or -1 if it
/// declares none.
int imageIndexForSlot(const MaterialData&    materialData,
                      const rnd::TextureSlot slot)
{
    switch (slot) {
    case rnd::TextureSlot::BaseColor: return materialData.baseColorImageIndex;
    case rnd::TextureSlot::MetallicRoughness:
        return materialData.metallicRoughnessImageIndex;
    case rnd::TextureSlot::Normal: return materialData.normalImageIndex;
    case rnd::TextureSlot::Occlusion: return materialData.occlusionImageIndex;
    case rnd::TextureSlot::Emissive: return materialData.emissiveImageIndex;
    }

    return -1;
}

/// Return the format the texture of the specified 'slot' must be uploaded
/// with. Base color and emissive images carry sRGB encoded colors, which the
/// hardware converts on every fetch; the other slots carry linear
/// measurements that must reach the shader untouched.
rhi::Format formatForSlot(const rnd::TextureSlot slot)
{
    switch (slot) {
    case rnd::TextureSlot::BaseColor:
    case rnd::TextureSlot::Emissive: return rhi::Format::R8G8B8A8_SRGB;
    case rnd::TextureSlot::MetallicRoughness:
    case rnd::TextureSlot::Normal:
    case rnd::TextureSlot::Occlusion: break;
    }

    return rhi::Format::R8G8B8A8_UNorm;
}

}  // close unnamed namespace

AssetManager::AssetManager(rhi::ContextProtocol* context,
                           rnd::Renderer*        renderer)
: d_context_p(context)
, d_renderer_p(renderer)
{
}

rnd::Texture* AssetManager::loadMaterial(const std::string& filePath,
                                         const rhi::Format  format)
{
    const std::string key = makeTextureKey(filePath, format);

    if (d_textures.contains(key)) {
        return d_textures[key].get();
    }

    auto texture = std::make_unique<rnd::Texture>(d_context_p,
                                                  filePath,
                                                  format);

    rnd::Texture* texturePtr = texture.get();
    d_textures[key]          = std::move(texture);

    return texturePtr;
}

rnd::Texture* AssetManager::loadImage(const ImageData&  image,
                                      const rhi::Format format)
{
    if (image.name.empty()) {
        return nullptr;
    }

    const std::string key = makeTextureKey(image.name, format);

    const auto it = d_textures.find(key);
    if (it != d_textures.end()) {
        return it->second.get();
    }

    std::unique_ptr<rnd::Texture> texture;

    try {
        if (!image.encoded.empty()) {
            texture = std::make_unique<rnd::Texture>(d_context_p,
                                                     image.encoded,
                                                     image.name,
                                                     format);
        }
        else if (!image.path.empty()) {
            texture = std::make_unique<rnd::Texture>(d_context_p,
                                                     image.path,
                                                     format);
        }
        else {
            return nullptr;
        }
    }
    catch (const std::exception& exception) {
        std::cerr << "[AssetManager] " << exception.what() << "\n";
        return nullptr;
    }

    rnd::Texture* texturePtr = texture.get();
    d_textures[key]          = std::move(texture);

    return texturePtr;
}

rnd::Texture* AssetManager::neutralTexture(const rnd::TextureSlot slot)
{
    // A tangent space normal of (0, 0, 1) is encoded as (128, 128, 255); the
    // other slots are neutral under multiplication, hence white. Sampling
    // white leaves the factor of the slot as the whole value, which is what
    // the specification prescribes for a texture that is absent.
    const bool isNormal = slot == rnd::TextureSlot::Normal;

    const std::array<unsigned char, 4> pixel =
        isNormal ? std::array<unsigned char, 4>{128, 128, 255, 255}
                 : std::array<unsigned char, 4>{255, 255, 255, 255};

    const std::string key = isNormal ? "#neutral_normal" : "#neutral_white";

    const auto it = d_textures.find(key);
    if (it != d_textures.end()) {
        return it->second.get();
    }

    auto texture = std::make_unique<rnd::Texture>(d_context_p,
                                                  pixel,
                                                  1,
                                                  1,
                                                  rhi::Format::R8G8B8A8_UNorm);

    rnd::Texture* texturePtr = texture.get();
    d_textures[key]          = std::move(texture);

    return texturePtr;
}

rnd::Material* AssetManager::createMaterial()
{
    auto           material    = std::make_unique<rnd::Material>(d_context_p);
    rnd::Material* materialPtr = material.get();

    // A newly created material is opaque and single sided; 'loadMesh'
    // reassigns the pipeline once it knows what the file declares.
    materialPtr->setPipeline(
        d_renderer_p->materialPipeline(rnd::AlphaMode::Opaque, false));
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
    else if (extension == ".gltf" || extension == ".glb") {
        importer = std::make_shared<impt::GltfImporter>();
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

    constexpr std::array slots = {rnd::TextureSlot::BaseColor,
                                  rnd::TextureSlot::MetallicRoughness,
                                  rnd::TextureSlot::Normal,
                                  rnd::TextureSlot::Occlusion,
                                  rnd::TextureSlot::Emissive};

    std::vector<rnd::Material*> uniqueMaterials;
    for (const MaterialData& materialData : data->materials) {
        rnd::Material* mat = createMaterial();

        auto resourceSet = d_context_p->createResourceSet(
            d_renderer_p->materialLayout());

        for (const rnd::TextureSlot slot : slots) {
            const int imageIndex = imageIndexForSlot(materialData, slot);

            rnd::Texture* tex = nullptr;

            if (imageIndex > -1 &&
                imageIndex < static_cast<int>(data->images.size())) {
                tex = loadImage(data->images[imageIndex], formatForSlot(slot));

                // The material meant to carry a texture here and the file
                // could not be read, which the checkerboard makes visible
                // rather than passing for a plain surface.
                if (!tex && slot == rnd::TextureSlot::BaseColor) {
                    tex = loadMaterial("textures/no_texture.png");
                }
            }

            // A slot left empty is not an error: the factor of that slot
            // then describes the material on its own.
            if (!tex) {
                tex = neutralTexture(slot);
            }

            mat->setTexture(slot, tex);
            resourceSet->updateTexture(static_cast<uint32_t>(slot),
                                       tex->protocol());
        }

        mat->setParams(toMaterialParams(materialData));
        mat->setAlphaMode(toRenderAlphaMode(materialData));
        mat->setDoubleSided(materialData.doubleSided);
        mat->setPipeline(d_renderer_p->materialPipeline(mat->alphaMode(),
                                                        mat->doubleSided()));

        resourceSet->updateBuffer(rnd::k_PARAMS_BINDING,
                                  mat->paramsUbo(),
                                  0,
                                  0);

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

        if (safeMatIndex < static_cast<int>(uniqueMaterials.size())) {
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