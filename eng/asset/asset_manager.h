// asset_manager.h                                                    -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_MANAGER_H
#define INCLUDED_ENG_ASSET_MANAGER_H

//@PURPOSE: Provide a central registry for loading and managing engine assets.
//
//@CLASSES:
//  eng::asset::LoadedModel: Container for imported geometry and materials.
//  eng::asset::AssetManager: Central class for resource lifecycle and caching.
//
//@DESCRIPTION: This component provides the 'eng::asset::AssetManager' class,
// which handles the importation of external files (3D models, textures) and
// their conversion into GPU-ready RHI resources. It maintains internal
// caches to ensure assets are reused and provides a unified API for
// material and mesh creation.

// std
#include <memory>
#include <unordered_map>
#include <vector>

// renderer
#include <rnd/material.h>
#include <rnd/mesh.h>
#include <rnd/renderer.h>
#include <rnd/texture.h>

// rhi
#include <rhi/rhi_contextprotocol.h>
#include <rhi/rhi_resourcelayoutprotocol.h>
#include <rhi/rhi_resourcesetprotocol.h>
#include <rhi/rhi_types.h>

namespace eng::asset {

// Forward declarations
struct ImageData;

/// This structure stores the set of meshes and materials imported from a
/// single model file.
struct LoadedModel {
    std::vector<std::shared_ptr<rnd::Mesh> > meshes;
    std::vector<rnd::Material*>              materials;
};

// ============
// class AssetManager
// ============

/// This class is responsible for loading, caching, and managing the lifetime
/// of textures, meshes, and materials.
class AssetManager {
  private:
    // DATA
    rhi::ContextProtocol* d_context_p;
    rnd::Renderer*        d_renderer_p;

    /// The textures loaded so far, keyed by the name of their image and the
    /// format they were uploaded with. The format belongs in the key because
    /// a single file read as a base color and as a roughness map yields two
    /// distinct GPU resources.
    std::unordered_map<std::string, std::unique_ptr<rnd::Texture> > d_textures;
    std::vector<std::unique_ptr<rnd::Material> >            d_materials;
    std::vector<std::unique_ptr<rhi::ResourceSetProtocol> > d_resourceSets;
    std::unordered_map<std::string, LoadedModel>            d_models;

  public:
    // CREATORS

    /// Create an 'AssetManager' using the specified 'context' and 'renderer'.
    /// The behavior is undefined unless 'context' and 'renderer' are
    /// non-null and remain valid for the lifetime of this manager.
    explicit AssetManager(rhi::ContextProtocol* context,
                          rnd::Renderer*        renderer);

    /// Destroy this manager and release all cached engine assets.
    ~AssetManager() = default;

    // MANIPULATORS

    /// Load and return a pointer to the texture at the specified 'filePath',
    /// giving its pixels the specified 'format' on the device. Return the
    /// cached texture if it was already loaded under 'format'.
    rnd::Texture*
    loadMaterial(const std::string& filePath,
                 rhi::Format        format = rhi::Format::R8G8B8A8_SRGB);

    /// Load and return a pointer to the texture described by the specified
    /// 'image', decoding its embedded bytes when it carries some, and reading
    /// its file otherwise, giving its pixels the specified 'format' on the
    /// device. Return the cached texture if 'image' was already loaded under
    /// 'format', and an empty pointer if it holds no loadable image.
    rnd::Texture* loadImage(const ImageData& image, rhi::Format format);

    /// Return a pointer to the single pixel texture standing in for the
    /// specified 'slot' when a material declares no texture for it: an image
    /// neutral for the factor of that slot, so that the factor alone
    /// describes the material.
    rnd::Texture* neutralTexture(rnd::TextureSlot slot);

    /// Create and return a pointer to a new empty material.
    rnd::Material* createMaterial();

    /// Load and return the model data from the specified 'filePath'.
    /// Return the cached model if it was already loaded.
    LoadedModel loadMesh(const std::string& filePath);
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_MANAGER_H