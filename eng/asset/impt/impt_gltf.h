// impt_gltf.h                                                        -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_IMPT_GLTF_H
#define INCLUDED_ENG_ASSET_IMPT_GLTF_H

//@PURPOSE: Provide an importer for .gltf/.glb files.
//
//@CLASSES:
//  eng::asset::impt::GltfImporter: Concrete importer for .gltf/.glb mesh
//  data.
//
//@DESCRIPTION: This component provides the 'eng::asset::impt::GltfImporter'
// class, which implements the 'ModelImporter' interface using the 'tinygltf'
// library to parse 3D geometry from .gltf and .glb files. Image data embedded
// in the file is deliberately ignored; textures are loaded separately by the
// renderer.

// std
#include <memory>
#include <string>

// asset
#include <asset/impt/impt_model.h>

namespace eng::asset::impt {

// ==================
// class GltfImporter
// ==================

/// This class is responsible for loading and parsing .gltf/.glb files.
class GltfImporter : public ModelImporter {
  public:
    // CREATORS

    /// Create a glTF importer.
    GltfImporter() = default;

    /// Destroy this importer.
    ~GltfImporter() override = default;

    // MANIPULATORS

    /// Load and return the model data from the specified 'filepath'. Return
    /// an empty pointer if the file is invalid or cannot be read.
    std::shared_ptr<ModelData> load(const std::string& filepath) override;
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_IMPT_GLTF_H
