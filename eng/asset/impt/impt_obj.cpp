// impt_obj.cpp                                                       -*-C++-*-
#include <asset/impt/impt_obj.h>

// std
#include <filesystem>
#include <unordered_map>

// asset
#include <asset/asset_modeldata.h>

// core
#include <core/core_log.h>
#include <core/core_vertex.h>

// third-party
#define TINYOBJLOADER_IMPLEMENTATION
#include <../../../third_party/tiny_obj/tiny_obj_loader.h>

namespace eng::asset::impt {

std::shared_ptr<ModelData> ObjImporter::load(const std::string& filepath)
{
    auto modelData = std::make_shared<ModelData>();

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;

    std::filesystem::path path(filepath);
    std::string           baseDir = path.parent_path().string();
    if (!baseDir.empty()) {
        baseDir += "/";
    }

    if (!tinyobj::LoadObj(&attrib,
                          &shapes,
                          &materials,
                          &err,
                          filepath.c_str(),
                          baseDir.c_str())) {
        LOG_ERROR("Failed to load model: ", err);
    }

    if (!err.empty()) {
        LOG_WARN("tinyobjloader warning: ", err);
    }

    std::unordered_map<core::Vertex, uint32_t> uniqueVertices{};
    std::vector<core::Vertex>                  vertices = {};
    std::vector<uint32_t>                      indices  = {};

    for (const auto& [name, mesh] : shapes) {
        for (const auto& [vertex_index, normal_index, texcoord_index] :
             mesh.indices) {
            core::Vertex vertex{};

            vertex.position = {attrib.vertices[3 * vertex_index + 0],
                               attrib.vertices[3 * vertex_index + 1],
                               attrib.vertices[3 * vertex_index + 2]};

            if (texcoord_index >= 0) {
                vertex.uv = {attrib.texcoords[2 * texcoord_index + 0],
                             1.0f - attrib.texcoords[2 * texcoord_index + 1]};
            }

            if (normal_index >= 0) {
                vertex.normal = {attrib.normals[3 * normal_index + 0],
                                 attrib.normals[3 * normal_index + 1],
                                 attrib.normals[3 * normal_index + 2]};
            }

            if (!uniqueVertices.contains(vertex)) {
                uniqueVertices[vertex] = static_cast<uint32_t>(
                    vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    modelData->meshes.emplace_back(vertices, indices);

    return modelData;
}

}  // close package namespace
