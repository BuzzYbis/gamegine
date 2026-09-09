// impt_gltf.cpp                                                      -*-C++-*-
#include <asset/impt/impt_gltf.h>

// core
#include <core/core_profiler.h>

// std
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// asset
#include <asset/asset_modeldata.h>

// core
#include <core/core_log.h>
#include <core/core_vertex.h>

// third-party
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
// The implementation of 'stb_image' is instantiated by 'eng::rnd::Texture',
// which this component links against; only its declarations are needed here.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ktx.h>
#include <tiny_gltf.h>

namespace eng::asset::impt {
namespace {

// ===================
// struct AccessorView
// ===================

/// This structure provides a non-owning view over the elements of a single
/// glTF accessor. The model holding the referenced buffer must outlive every
/// view created on it.
struct AccessorView {
    // DATA
    const unsigned char* d_data_p;  // first element (held, not owned)
    int                  d_stride;  // distance in bytes between elements
    std::size_t          d_count;   // number of elements
};

/// Return the local transformation matrix of the specified 'node', taken
/// from its matrix when present, and composed from its translation, rotation
/// and scale components otherwise.
glm::mat4 computeLocalTransform(const tinygltf::Node& node)
{
    glm::mat4 localTransform;

    // Check if the matrix exist (the matrix length must be 0 or 16)
    if (node.matrix.size() == 16) {
        localTransform = glm::make_mat4(node.matrix.data());
    }
    else {
        // If the matrix don't exist we create a local transform by using
        // translation, rotation and scale vectors
        glm::vec3 translation = glm::vec3(0.0F);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // w, x, y, z
        glm::vec3 scale    = glm::vec3(1.0f);

        if (node.translation.size() == 3) {
            translation = glm::make_vec3(node.translation.data());
        }

        // glTF rotation on node use quaternion
        if (node.rotation.size() == 4) {
            rotation = glm::make_quat(node.rotation.data());
        }

        if (node.scale.size() == 3) {
            scale = glm::make_vec3(node.scale.data());
        }

        // Create local transform: T * R * S
        localTransform = glm::translate(glm::mat4(1.0f), translation) *
                         glm::mat4_cast(rotation) *
                         glm::scale(glm::mat4(1.0f), scale);
    }
    return localTransform;
}

/// Return a view over the elements of the accessor at the specified
/// 'accessorIndex' in the specified 'model'. Throw 'std::runtime_error'
/// unless 'accessorIndex' designates an accessor backed by a valid buffer
/// view. The behavior is undefined unless 'model' outlives the returned
/// view.
AccessorView makeAccessorView(const tinygltf::Model& model,
                              const int              accessorIndex)
{
    if (accessorIndex < 0 ||
        accessorIndex >= static_cast<int>(model.accessors.size())) {
        throw std::runtime_error("glTF accessor index is out of range");
    }

    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];

    if (accessor.bufferView < 0 ||
        accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        throw std::runtime_error("glTF accessor has no valid buffer view");
    }

    const tinygltf::BufferView& bufferView =
        model.bufferViews[accessor.bufferView];

    if (bufferView.buffer < 0 ||
        bufferView.buffer >= static_cast<int>(model.buffers.size())) {
        throw std::runtime_error("glTF buffer view has no valid buffer");
    }

    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    return {
        .d_data_p = buffer.data.data() + bufferView.byteOffset +
                    accessor.byteOffset,
        .d_stride = accessor.ByteStride(bufferView),
        .d_count  = accessor.count,
    };
}

/// Return the vertices of the specified 'primitive' of the specified
/// 'model', with every position transformed by the specified
/// 'worldTransform' and every normal transformed by its inverse transpose.
/// Throw 'std::runtime_error' if 'primitive' has no position attribute or if
/// one of its attributes cannot be read.
std::vector<core::Vertex> readVertices(const tinygltf::Model&     model,
                                       const tinygltf::Primitive& primitive)
{
    const auto& attributes = primitive.attributes;
    const auto  posIt      = attributes.find("POSITION");

    if (posIt == attributes.end()) {
        throw std::runtime_error("glTF primitive has no POSITION attribute");
    }

    const AccessorView posView = makeAccessorView(model, posIt->second);

    std::optional<AccessorView> normView;
    std::optional<AccessorView> uvView;

    const auto normIt = attributes.find("NORMAL");
    if (normIt != attributes.end()) {
        normView = makeAccessorView(model, normIt->second);
    }

    const auto uvIt = attributes.find("TEXCOORD_0");
    if (uvIt != attributes.end()) {
        uvView = makeAccessorView(model, uvIt->second);
    }

    std::vector<core::Vertex> vertices(posView.d_count);

    for (std::size_t i = 0; i < posView.d_count; ++i) {
        core::Vertex&        vertex = vertices[i];
        const unsigned char* posPtr = posView.d_data_p + i * posView.d_stride;
        const float*         pos    = reinterpret_cast<const float*>(posPtr);

        vertex.position = glm::vec3(pos[0], pos[1], pos[2]);

        if (normView.has_value()) {
            const unsigned char* normPtr = normView->d_data_p +
                                           i * normView->d_stride;
            const float* norm = reinterpret_cast<const float*>(normPtr);
            vertex.normal     = glm::normalize(
                glm::vec3(norm[0], norm[1], norm[2]));
        }

        if (uvView.has_value()) {
            const unsigned char* uvPtr = uvView->d_data_p +
                                         i * uvView->d_stride;
            const float*         uv    = reinterpret_cast<const float*>(uvPtr);
            vertex.uv                  = {uv[0], 1.0F - uv[1]};
        }
    }

    return vertices;
}

/// Return the index of the specified 'componentType' read at the specified
/// 'ptr'. Throw 'std::runtime_error' unless 'componentType' designates an
/// unsigned integral glTF component type.
uint32_t readIndex(const int componentType, const unsigned char* ptr)
{
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return *reinterpret_cast<const uint32_t*>(ptr);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return *reinterpret_cast<const uint16_t*>(ptr);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return *ptr;
    default: throw std::runtime_error("Unsupported index component type");
    }
}

/// Return the indices of the specified 'primitive' of the specified 'model'.
/// Throw 'std::runtime_error' if the index accessor cannot be read or if its
/// component type is not supported.
std::vector<uint32_t> readIndices(const tinygltf::Model&     model,
                                  const tinygltf::Primitive& primitive)
{
    const AccessorView indexView = makeAccessorView(model, primitive.indices);
    const int componentType = model.accessors[primitive.indices].componentType;
    std::vector<uint32_t> indices(indexView.d_count);

    for (std::size_t i = 0; i < indexView.d_count; ++i) {
        const unsigned char* ptr = indexView.d_data_p + i * indexView.d_stride;
        const uint32_t       index = readIndex(componentType, ptr);
        indices[i]                 = index;
    }

    return indices;
}

/// Return 'true' if the pixels of the specified 'image' are stored inside
/// the glTF file itself, either in a buffer view or as a data URI, and
/// 'false' if they live in a separate file.
bool isEmbeddedImage(const tinygltf::Image& image)
{
    return image.uri.empty() || tinygltf::IsDataURI(image.uri);
}

/// Return the images of the specified 'model', in that same order, the ones
/// stored in a separate file carrying its path resolved relatively to the
/// specified 'modelPath', and the embedded ones carrying their encoded bytes.
/// Every returned image carries a name unique to 'modelPath', so that two
/// models cannot collide in the texture cache of the asset manager.
std::vector<ImageData> readImages(const tinygltf::Model&       model,
                                  const std::filesystem::path& modelPath)
{
    const std::filesystem::path baseDir = modelPath.parent_path();

    std::vector<ImageData> images(model.images.size());

    for (std::size_t i = 0; i < model.images.size(); ++i) {
        const tinygltf::Image& image = model.images[i];

        ImageData imageData = {};

        if (isEmbeddedImage(image)) {
            // An embedded image has no file of its own to be named after.
            imageData.name = modelPath.string() + "#image" + std::to_string(i);

            imageData.encoded = image.image;
        }
        else {
            // A glTF URI is percent encoded, whereas the filesystem expects
            // the decoded form.
            std::string uri;

            if (tinygltf::URIDecode(image.uri, &uri, nullptr)) {
                imageData.path = (baseDir / uri).string();

                // Naming an external image after its own path lets two
                // models sharing a texture file share its GPU resource.
                imageData.name = imageData.path;
            }
            else {
                LOG_WARN("Failed to decode the URI of glTF image: ",
                         image.uri);
            }
        }
        images[i] = std::move(imageData);
    }
    return images;
}

/// Return the index, in the image collection of the specified 'model', of
/// the image sourced by the texture at the specified 'textureIndex', or -1 if
/// 'textureIndex' designates no valid image. A glTF texture designates its
/// pixels indirectly, through an image and a sampler, hence the extra hop.
int resolveImageIndex(const tinygltf::Model& model, const int textureIndex)
{
    if (textureIndex < 0 ||
        textureIndex >= static_cast<int>(model.textures.size())) {
        return -1;
    }

    const int imageIndex = model.textures[textureIndex].source;

    if (imageIndex < 0 ||
        imageIndex >= static_cast<int>(model.images.size())) {
        return -1;
    }

    return imageIndex;
}

/// Return the alpha mode designated by the specified 'mode', or
/// 'AlphaMode::Opaque' if 'mode' designates none, that being the default
/// mandated by the glTF specification.
AlphaMode toAlphaMode(const std::string& mode)
{
    if (mode == "MASK") {
        return AlphaMode::Mask;
    }

    if (mode == "BLEND") {
        return AlphaMode::Blend;
    }

    return AlphaMode::Opaque;
}

/// Overwrite in the specified 'materialData' the base color, metallic and
/// roughness properties with those declared by the
/// 'KHR_materials_pbrSpecularGlossiness' extension of the specified
/// 'material' of the specified 'model', if that extension is present. An
/// asset converted from the specular-glossiness workflow carries its color
/// texture in that extension and leaves the core metallic-roughness object
/// empty, so the extension takes precedence when declared.
void applySpecularGlossiness(const tinygltf::Model&    model,
                             const tinygltf::Material& material,
                             MaterialData&             materialData)
{
    const auto extensionIt = material.extensions.find(
        "KHR_materials_pbrSpecularGlossiness");

    if (extensionIt == material.extensions.end()) {
        return;
    }

    const tinygltf::Value& specGloss = extensionIt->second;

    if (specGloss.Has("diffuseTexture")) {
        const tinygltf::Value& texture = specGloss.Get("diffuseTexture");

        if (texture.Has("index")) {
            materialData.baseColorImageIndex = resolveImageIndex(
                model,
                texture.Get("index").GetNumberAsInt());
        }
    }

    if (specGloss.Has("diffuseFactor")) {
        const tinygltf::Value& factor = specGloss.Get("diffuseFactor");

        if (factor.IsArray() && factor.ArrayLen() == 4) {
            for (int i = 0; i < 4; ++i) {
                materialData.baseColorFactor[i] = static_cast<float>(
                    factor.Get(i).GetNumberAsDouble());
            }
        }
    }

    // The specular-glossiness workflow describes a diffuse surface, and
    // glossiness is the complement of roughness.
    materialData.metallicFactor = 0.0f;

    if (specGloss.Has("glossinessFactor")) {
        materialData.roughnessFactor =
            1.0f - static_cast<float>(
                       specGloss.Get("glossinessFactor").GetNumberAsDouble());
    }
}

/// Set in the specified 'materialData' the transmission declared by the
/// 'KHR_materials_transmission' extension of the specified 'material', if
/// that extension is present. An asset describes glass through that
/// extension rather than through its alpha, which it leaves at the opaque
/// default of the core specification; a material declaring transmission is
/// therefore seen through whatever alpha mode it reads as.
void applyTransmission(const tinygltf::Material& material,
                       MaterialData&             materialData)
{
    const auto extensionIt = material.extensions.find(
        "KHR_materials_transmission");

    if (extensionIt == material.extensions.end()) {
        return;
    }

    const tinygltf::Value& transmission = extensionIt->second;

    if (transmission.Has("transmissionFactor")) {
        materialData.transmissionFactor = static_cast<float>(
            transmission.Get("transmissionFactor").GetNumberAsDouble());
    }
}

std::vector<MeshData>
readMeshes(const tinygltf::Model&               model,
           std::vector<std::vector<uint32_t> >& primitiveMeshMap)
{
    std::vector<MeshData> meshes;

    for (size_t m = 0; m < model.meshes.size(); ++m) {
        const auto& meshe = model.meshes[m];
        primitiveMeshMap[m].reserve(meshe.primitives.size());
        for (size_t p = 0; p < meshe.primitives.size(); ++p) {
            MeshData    meshData;
            const auto& primitive = meshe.primitives[p];

            meshData.vertices = readVertices(model, primitive);

            if (primitive.indices > -1) {
                meshData.indices = readIndices(model, primitive);
            }

            uint32_t meshIndex     = static_cast<uint32_t>(meshes.size());
            meshData.materialIndex = primitive.material;
            primitiveMeshMap[m][p] = meshIndex;
            meshes.emplace_back(meshData);
        }
    }

    return meshes;
}

/// Return the materials of the specified 'model', in that same order, each
/// carrying the factors of the metallic-roughness model and referring to its
/// images by index. A property left undeclared by 'model' keeps the default
/// value mandated by the glTF specification.
std::vector<MaterialData> readMaterials(const tinygltf::Model& model)
{
    std::vector<MaterialData> materials;
    materials.reserve(model.materials.size());

    for (const tinygltf::Material& material : model.materials) {
        const tinygltf::PbrMetallicRoughness& pbr =
            material.pbrMetallicRoughness;

        MaterialData materialData;

        materialData.name = material.name;

        materialData.baseColorFactor = glm::make_vec4(
            pbr.baseColorFactor.data());
        materialData.emissiveFactor = glm::make_vec3(
            material.emissiveFactor.data());

        materialData.metallicFactor  = static_cast<float>(pbr.metallicFactor);
        materialData.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

        materialData.normalScale = static_cast<float>(
            material.normalTexture.scale);
        materialData.occlusionStrength = static_cast<float>(
            material.occlusionTexture.strength);

        materialData.baseColorImageIndex =
            resolveImageIndex(model, pbr.baseColorTexture.index);
        materialData.metallicRoughnessImageIndex =
            resolveImageIndex(model, pbr.metallicRoughnessTexture.index);
        materialData.normalImageIndex =
            resolveImageIndex(model, material.normalTexture.index);
        materialData.occlusionImageIndex =
            resolveImageIndex(model, material.occlusionTexture.index);
        materialData.emissiveImageIndex =
            resolveImageIndex(model, material.emissiveTexture.index);

        applySpecularGlossiness(model, material, materialData);
        applyTransmission(material, materialData);

        materialData.alphaMode   = toAlphaMode(material.alphaMode);
        materialData.alphaCutoff = static_cast<float>(material.alphaCutoff);
        materialData.doubleSided = material.doubleSided;

        materials.push_back(std::move(materialData));
    }

    return materials;
}

/// Append to the specified 'modelData' the meshes of the node at the
/// specified 'nodeIndex' in the specified 'model' and those of all its
/// descendants, each transformed by the specified 'parentTransform' combined
/// with the local transform of the node. Throw 'std::runtime_error' if
/// 'nodeIndex', or any index reachable from it, does not designate a valid
/// element of 'model'.
void processNode(const tinygltf::Model&               model,
                 int                                  nodeIndex,
                 const glm::mat4&                     parentTransform,
                 ModelData&                           modelData,
                 std::vector<std::vector<uint32_t> >& primitiveMeshMap)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) {
        throw std::runtime_error("glTF node index is out of range");
    }

    const tinygltf::Node& node           = model.nodes[nodeIndex];
    const glm::mat4       localTransform = computeLocalTransform(node);
    const glm::mat4       worldTransform = parentTransform * localTransform;

    if (node.mesh > -1) {
        if (node.mesh >= static_cast<int>(model.meshes.size())) {
            throw std::runtime_error("glTF mesh index is out of range");
        }

        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        for (size_t p = 0; p < mesh.primitives.size(); ++p) {
            InstanceData instance{};
            instance.meshDataIndex  = primitiveMeshMap[node.mesh][p];
            instance.materialIndex  = mesh.primitives[p].material;
            instance.worldTransform = worldTransform;
            modelData.instanceData.push_back(instance);
        }
    }

    // Recurse on children
    for (const int childIndex : node.children) {
        processNode(model,
                    childIndex,
                    worldTransform,
                    modelData,
                    primitiveMeshMap);
    }
}

}  // close unnamed namespace

// ==================
// class GltfImporter
// ==================

// MANIPULATORS
std::shared_ptr<ModelData> GltfImporter::load(const std::string& filepath)
{
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err;
    std::string        warn;

    // Disable automatic image decoding by tinyglf
    loader.SetImagesAsIs(true);

    const std::filesystem::path path(filepath);
    bool                        result;

    {
        ENG_PROFILE_SCOPE("Load Parse");

        if (path.extension() == ".glb") {
            result = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
        }
        else {
            result = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
        }
    }

    if (!warn.empty()) {
        LOG_WARN("tinygltf warning: ", warn);
    }

    if (!err.empty()) {
        LOG_ERROR("tinygltf error: ", err);
    }

    if (!result) {
        LOG_ERROR("Failed to load model: ", filepath);
        return nullptr;
    }

    const int sceneIndex = model.defaultScene > -1 ? model.defaultScene : 0;

    if (sceneIndex >= static_cast<int>(model.scenes.size())) {
        LOG_ERROR("No scene to import in model: ", filepath);
        return nullptr;
    }

    std::shared_ptr<ModelData> modelData = std::make_shared<ModelData>();

    std::vector<std::vector<uint32_t> > primitiveMeshMap(model.meshes.size());

    {
        ENG_PROFILE_SCOPE("Load Geometry");
        modelData->meshes = readMeshes(model, primitiveMeshMap);
    }

    {
        ENG_PROFILE_SCOPE("Load Material Data");
        modelData->materials = readMaterials(model);
    }

    // Only the images the file carries itself are read here; the ones it
    // merely points at are opened by the texture that decodes them, and
    // hence weigh on 'Load Textures' instead.
    {
        ENG_PROFILE_SCOPE("Load Image Scan");
        modelData->images = readImages(model, path);
    }

    constexpr glm::mat4    rootTransform = glm::mat4(1.0f);
    const tinygltf::Scene& scene         = model.scenes[sceneIndex];

    try {
        ENG_PROFILE_SCOPE("Load Node Graph");

        for (const int nodeIndex : scene.nodes) {
            processNode(model,
                        nodeIndex,
                        rootTransform,
                        *modelData,
                        primitiveMeshMap);
        }
    }
    catch (const std::exception& exception) {
        LOG_ERROR("Failed to import model ", filepath, ": ", exception.what());
        return nullptr;
    }

    return modelData;
}

}  // close package namespace
