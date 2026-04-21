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

namespace eng::asset {

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

    std::unique_ptr<rhi::ResourceLayoutProtocol> d_materialLayout;
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

    /// Load and return a pointer to the texture at the specified 'filePath'.
    /// Return the cached texture if it was already loaded.
    rnd::Texture* loadMaterial(const std::string& filePath);

    /// Create and return a pointer to a new empty material.
    rnd::Material* createMaterial();

    /// Load and return the model data from the specified 'filePath'.
    /// Return the cached model if it was already loaded.
    LoadedModel loadMesh(const std::string& filePath);
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_MANAGER_H