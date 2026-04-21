// impt_model.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_IMPT_MODEL_H
#define INCLUDED_ENG_ASSET_IMPT_MODEL_H

//@PURPOSE: Provide an interface for importing 3D models.
//
//@CLASSES:
//  eng::asset::impt::ModelImporter: Abstract base class for format-specific
//  importers.
//
//@DESCRIPTION: This component provides the 'eng::asset::ModelImporter'
// interface, which defines the standard API for loading 3D mesh and material
// data from external files into the engine's internal format.

// std
#include <memory>
#include <string>

// Forward declarations
namespace eng::asset {
struct ModelData;
}  // close package namespace

namespace eng::asset::impt {
// ===================
// class ModelImporter
// ===================

/// This class provides a protocol for importing 3D model data from disk.
class ModelImporter {
  public:
    // CREATORS

    /// Destroy this importer.
    virtual ~ModelImporter() = default;

    // MANIPULATORS

    /// Load and return the model data from the specified 'filepath'. Return
    /// an empty pointer if the loading fails.
    virtual std::shared_ptr<ModelData> load(const std::string& filepath) = 0;
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_IMPT_MODEL_H
