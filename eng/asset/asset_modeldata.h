// asset_modeldata.h                                                  -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_MODELDATA_H
#define INCLUDED_ENG_ASSET_MODELDATA_H

//@PURPOSE: Provide data structures for intermediate 3D model storage.
//
//@CLASSES:
//  eng::asset::AlphaMode: How the alpha of a material is interpreted.
//  eng::asset::MaterialData: Raw material parameters from a file.
//  eng::asset::ImageData: Raw image bytes or path parsed from a file.
//  eng::asset::MeshData: Geometric data for a single mesh part.
//  eng::asset::InstanceData: One placement of a mesh inside a model.
//  eng::asset::ModelData: Collection of meshes and materials forming a model.
//
//@DESCRIPTION: This component provides Plain Old Data (POD) structures used
// by importers to store parsed 3D data before it is converted into GPU
// resources by the AssetManager.
//
// 'MaterialData' follows the metallic-roughness model of glTF: every texture
// is modulated by a factor of the same name, and a material carrying no
// texture at all is fully described by its factors. The default value of
// every member is the one mandated by the glTF specification, so that a
// default constructed 'MaterialData' matches a material declaring no
// property.

// std
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// renderer
#include <rnd/mesh.h>

// third-party
#include <glm/glm.hpp>

namespace eng::asset {

// ==============
// enum AlphaMode
// ==============

/// This enumeration lists the ways the alpha channel of a material can be
/// interpreted by the renderer.
enum class AlphaMode {
    Opaque,  // alpha is ignored, the surface is fully opaque
    Mask,    // the fragment is discarded below the alpha cutoff
    Blend    // alpha is composited with the destination
};

// ===================
// struct MaterialData
// ===================

/// This structure stores raw material information parsed from a model file.
struct MaterialData {
    std::string name;

    // Factors multiplying the texture of the same name, and holding the
    // whole value when that texture is absent.
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec3 emissiveFactor  = glm::vec3(0.0f);
    float     metallicFactor  = 1.0f;
    float     roughnessFactor = 1.0f;

    // Modifiers of their own texture, meaningless without it.
    float normalScale       = 1.0f;
    float occlusionStrength = 1.0f;

    // Fraction of the light reaching the surface that passes through it. A
    // material declaring no transmission keeps the zero of a surface nothing
    // sees through.
    float transmissionFactor = 0.0f;

    // Indices into 'ModelData::images', -1 when the texture is absent. The
    // metallic-roughness image packs occlusion, roughness and metalness in
    // its red, green and blue channels respectively.
    int baseColorImageIndex         = -1;
    int metallicRoughnessImageIndex = -1;
    int normalImageIndex            = -1;
    int occlusionImageIndex         = -1;
    int emissiveImageIndex          = -1;

    AlphaMode alphaMode   = AlphaMode::Opaque;
    float     alphaCutoff = 0.5f;
    bool      doubleSided = false;
};

// ===================
// struct ImageData
// ===================

/// This structure stores raw image information parsed from a model file.
struct ImageData {
    std::string                name;
    std::string                path;
    std::vector<unsigned char> encoded;
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

// ===================
// struct InstanceData
// ===================

/// This structure stores one placement of one mesh inside a model.
struct InstanceData {
    uint32_t  meshDataIndex  = 0;
    uint32_t  materialIndex  = 0;
    glm::mat4 worldTransform = glm::mat4(1.0f);
};

// ================
// struct ModelData
// ================

/// This structure represents the complete set of data imported from a 3D
/// model file.
struct ModelData {
    std::vector<MeshData>     meshes;
    std::vector<InstanceData> instanceData;
    std::vector<MaterialData> materials;
    std::vector<ImageData>    images;
};

}  // close package namespace
#endif  // INCLUDED_ENG_ASSET_MODELDATA_H
