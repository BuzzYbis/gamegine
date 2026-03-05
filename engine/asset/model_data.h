// model_data.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_ASSET_MODEL_DATA_H
#define INCLUDED_ENGINE_ASSET_MODEL_DATA_H

// renderer
#include <renderer/material.h>
#include <renderer/mesh.h>

// std
#include <vector>

namespace engine::asset {

struct MaterialData {
    std::string name;
    std::string diffuseTexturePath;
};

struct MeshData {
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t>     indices;
    int                       materialIndex = -1;
};

struct ModelData {
    std::vector<MeshData>     meshes;
    std::vector<MaterialData> materials;
};

}  // close engine::asset namespace

#endif  // INCLUDED_ENGINE_ASSET_MODEL_DATA_H
