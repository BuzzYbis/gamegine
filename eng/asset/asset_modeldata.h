// asset_modeldata.h                                                  -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_MODELDATA_H
#define INCLUDED_ENG_ASSET_MODELDATA_H

//@PURPOSE: Provide data structures for intermediate 3D model storage.
//
//@CLASSES:
//  eng::asset::MaterialData: Raw material parameters from a file.
//  eng::asset::MeshData: Geometric data for a single mesh part.
//  eng::asset::ModelData: Collection of meshes and materials forming a model.
//
//@DESCRIPTION: This component provides Plain Old Data (POD) structures used
// by importers to store parsed 3D data before it is converted into GPU
// resources by the AssetManager.

// std
#include <string>
#include <vector>

// renderer
#include <rnd/mesh.h>

namespace eng::asset {

// ===================
// struct MaterialData
// ===================

/// This structure stores raw material information parsed from a model file.
struct MaterialData {
    std::string name;
    std::string diffuseTexturePath;
};

// ===============
// struct MeshData
// ===============

/// This structure stores the vertices and indices for a single part of a
/// larger model.
struct MeshData {
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t>     indices;
    int                       materialIndex = -1;
};

// ================
// struct ModelData
// ================

/// This structure represents the complete set of data imported from a 3D
/// model file.
struct ModelData {
    std::vector<MeshData>     meshes;
    std::vector<MaterialData> materials;
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_MODELDATA_H
