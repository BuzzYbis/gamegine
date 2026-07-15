// impt_obj.cpp                                                       -*-C++-*-
#include <asset/impt/impt_obj.h>

// std
#include <algorithm>
#include <filesystem>
#include <unordered_map>

// asset
#include <asset/asset_modeldata.h>

// core
#include <core/core_log.h>
#include <core/core_vertex.h>

// third-party
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace eng::asset::impt {

namespace {

struct TempMesh {
    std::unordered_map<core::Vertex, uint32_t> uniqueVertices{};
    std::vector<core::Vertex>                  vertices = {};
    std::vector<uint32_t>                      indices  = {};
};

}  // close unnamed namespace

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

    if (!LoadObj(&attrib,
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

    std::unordered_map<int, TempMesh> materialGroups;

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            const size_t fv = shape.mesh.num_face_vertices[f];

            int matId          = shape.mesh.material_ids[f];
            int effectiveMatId = matId < 0 ? 0 : matId;

            auto& [uniqueVertices,
                   vertices,
                   indices] = materialGroups[effectiveMatId];

            // Optimization: Reserve memory for the material group
            if (indices.empty()) {
                indices.reserve(shape.mesh.indices.size());
                vertices.reserve(shape.mesh.indices.size() / 2);
            }

            // Calculate face normal if missing
            glm::vec3 faceNormal(0.0f);
            if (shape.mesh.indices[indexOffset].normal_index < 0 && fv >= 3) {
                tinyobj::index_t idx0 = shape.mesh.indices[indexOffset + 0];
                tinyobj::index_t idx1 = shape.mesh.indices[indexOffset + 1];
                tinyobj::index_t idx2 = shape.mesh.indices[indexOffset + 2];

                glm::vec3 v0 = {attrib.vertices[3 * idx0.vertex_index + 0],
                                attrib.vertices[3 * idx0.vertex_index + 1],
                                attrib.vertices[3 * idx0.vertex_index + 2]};
                glm::vec3 v1 = {attrib.vertices[3 * idx1.vertex_index + 0],
                                attrib.vertices[3 * idx1.vertex_index + 1],
                                attrib.vertices[3 * idx1.vertex_index + 2]};
                glm::vec3 v2 = {attrib.vertices[3 * idx2.vertex_index + 0],
                                attrib.vertices[3 * idx2.vertex_index + 1],
                                attrib.vertices[3 * idx2.vertex_index + 2]};

                faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            }

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                core::Vertex     vertex{};
                vertex.position = {attrib.vertices[3 * idx.vertex_index + 0],
                                   attrib.vertices[3 * idx.vertex_index + 1],
                                   attrib.vertices[3 * idx.vertex_index + 2]};

                if (idx.texcoord_index >= 0) {
                    vertex.uv = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]};
                }
                else {
                    vertex.uv = {0.0f, 0.0f};
                }

                if (idx.normal_index >= 0) {
                    vertex.normal = {attrib.normals[3 * idx.normal_index + 0],
                                     attrib.normals[3 * idx.normal_index + 1],
                                     attrib.normals[3 * idx.normal_index + 2]};
                }
                else {
                    vertex.normal = faceNormal;
                }

                if (!uniqueVertices.contains(vertex)) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(
                        vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
            indexOffset += fv;
        }
    }

    for (const auto& material : materials) {
        MaterialData materialData;
        materialData.name = material.name;
        if (!material.diffuse_texname.empty()) {
            std::string texPath = material.diffuse_texname;
            std::ranges::replace(texPath, '\\', '/');
            materialData.diffuseTexturePath = baseDir + texPath;
        }

        modelData->materials.push_back(materialData);
    }

    for (auto& [matId, group] : materialGroups) {
        modelData->meshes.emplace_back(std::move(group.vertices),
                                       std::move(group.indices),
                                       matId);
    }

    return modelData;
}

}  // close package namespace
