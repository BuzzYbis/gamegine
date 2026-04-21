// impt_obj.h                                                         -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_IMPT_OBJ_H
#define INCLUDED_ENG_ASSET_IMPT_OBJ_H

//@PURPOSE: Provide an importer for Wavefront .obj files.
//
//@CLASSES:
//  eng::asset::impt::ObjImporter: Concrete importer for .obj mesh data.
//
//@DESCRIPTION: This component provides the 'eng::asset::impt::ObjImporter'
// class, which implements the 'ModelImporter' interface using the
// 'tinyobjloader' library to parse 3D geometry and material libraries from
// .obj files.

// std
#include <string>

// asset
#include <asset/impt/impt_model.h>

namespace eng::asset::impt {

// =================
// class ObjImporter
// =================

/// This class is responsible for loading and parsing Wavefront .obj files.
class ObjImporter : public ModelImporter {
  public:
    // CREATORS

    /// Create an .obj importer.
    ObjImporter() = default;

    /// Destroy this importer.
    ~ObjImporter() override = default;

    // MANIPULATORS

    /// Load and return the model data from the specified 'filepath'. Return
    /// an empty pointer if the file is invalid or cannot be read.
    std::shared_ptr<ModelData> load(const std::string& filepath) override;
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_IMPT_OBJ_H
