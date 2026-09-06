// material.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_RND_MATERIAL_H
#define INCLUDED_ENG_RND_MATERIAL_H

//@PURPOSE: Provide a high-level representation of a surface material.
//
//@CLASSES:
//  eng::rnd::TextureSlot: Role of a texture in the metallic-roughness model.
//  eng::rnd::AlphaMode: How the alpha of a material is interpreted.
//  eng::rnd::MaterialParams: Factors of a material, laid out for the shader.
//  eng::rnd::Material: Encapsulates shader state and texture resources.
//
//@DESCRIPTION: This component provides a high-level 'Material' class that
// links a graphics pipeline (shaders, state) with specific resource sets
// (textures, uniforms) required to render a mesh.
//
// A material follows the metallic-roughness model: it holds one texture per
// role of 'TextureSlot', and a 'MaterialParams' whose every factor modulates
// the texture of the same name. A material carrying no texture at all is
// fully described by its factors, which is why every one of them defaults to
// the neutral value of the glTF specification.
//
// A material owns the uniform buffer holding its factors, and uploads them
// on every call to 'setParams'. It does not own its textures, its pipeline,
// nor its resource set, all of which are held elsewhere and must outlive it.

// std
#include <array>
#include <cstddef>
#include <memory>

// renderer
#include <rnd/texture.h>

// rhi
#include <rhi/rhi_bufferprotocol.h>
#include <rhi/rhi_contextprotocol.h>

// third-party
#include <glm/glm.hpp>

namespace eng::rnd {

// ================
// enum TextureSlot
// ================

/// This enumeration lists the roles a texture can play in the
/// metallic-roughness model. The value of an enumerator is both the index of
/// its texture in a material and the binding index of that texture in the
/// material resource set.
enum class TextureSlot : uint32_t {
    BaseColor         = 0,  // albedo, in sRGB space
    MetallicRoughness = 1,  // roughness in green, metalness in blue
    Normal            = 2,  // tangent space normals
    Occlusion         = 3,  // ambient occlusion in red
    Emissive          = 4   // emitted color, in sRGB space
};

/// The number of texture slots a material holds.
constexpr std::size_t k_TEXTURE_SLOT_COUNT = 5;

/// The binding index, in the material resource set, of the uniform buffer
/// holding the factors of a material. It follows the texture slots, so that
/// the whole contract with the shader is described by this component.
constexpr uint32_t k_PARAMS_BINDING = k_TEXTURE_SLOT_COUNT;

// ==============
// enum AlphaMode
// ==============

/// This enumeration lists the ways the alpha channel of a material can be
/// interpreted. It selects the pipeline a material is drawn with, and is
/// therefore held apart from the factors uploaded to the shader.
enum class AlphaMode {
    Opaque,  // alpha is ignored, the surface is fully opaque
    Mask,    // the fragment is discarded below the alpha cutoff
    Blend    // alpha is composited with the destination
};

// ====================
// struct MaterialParams
// ====================

/// This structure holds the factors of a material. Its members are ordered
/// so that their offsets match the std140 layout of the corresponding
/// uniform block, 'metallicFactor' filling the padding that follows the
/// three components of 'emissiveFactor'.
struct MaterialParams {
    glm::vec4 baseColorFactor    = glm::vec4(1.0f);  // offset 0
    glm::vec3 emissiveFactor     = glm::vec3(0.0f);  // offset 16
    float     metallicFactor     = 1.0f;             // offset 28
    float     roughnessFactor    = 1.0f;             // offset 32
    float     normalScale        = 1.0f;             // offset 36
    float     occlusionStrength  = 1.0f;             // offset 40
    float     alphaCutoff        = 0.5f;             // offset 44
    float     transmissionFactor = 0.0f;             // offset 48
};

// ==============
// class Material
// ==============

/// High-level representation of a surface material.
class Material {
  private:
    // DATA
    rhi::PipelineProtocol*    d_pipeline_p    = nullptr;
    rhi::ResourceSetProtocol* d_resourceSet_p = nullptr;

    /// The textures of this material, indexed by 'TextureSlot'. A slot holds
    /// an empty pointer when this material declares no texture for it.
    std::array<Texture*, k_TEXTURE_SLOT_COUNT> d_textures_p = {};

    /// The uniform buffer holding 'd_params' on the device.
    std::unique_ptr<rhi::BufferProtocol> d_paramsUbo;

    MaterialParams d_params = {};

    AlphaMode d_alphaMode   = AlphaMode::Opaque;
    bool      d_doubleSided = false;

  public:
    // CREATORS

    /// Create a material whose factors are neutral and whose texture slots
    /// are all empty, allocating its uniform buffer from the specified RHI
    /// 'context'. The behavior is undefined unless 'context' is non-null.
    explicit Material(rhi::ContextProtocol* context);

    /// Destroy this material and release its uniform buffer.
    ~Material() = default;

    // MANIPULATORS

    /// Set the graphics pipeline for this material to the specified
    /// 'pipeline'.
    void setPipeline(rhi::PipelineProtocol* pipeline);

    /// Set the resource set (uniforms, textures) for this material to the
    /// specified 'set'.
    void setResourceSet(rhi::ResourceSetProtocol* set);

    /// Set the texture of the specified 'slot' of this material to the
    /// specified 'texture'.
    void setTexture(TextureSlot slot, Texture* texture);

    /// Set the factors of this material to the specified 'params' and upload
    /// them to its uniform buffer.
    void setParams(const MaterialParams& params);

    /// Set the alpha mode of this material to the specified 'mode'.
    void setAlphaMode(AlphaMode mode);

    /// Set whether this material is drawn on both of its faces to the
    /// specified 'doubleSided'.
    void setDoubleSided(bool doubleSided);

    // ACCESSORS

    /// Return a pointer to the graphics pipeline associated with this
    /// material.
    [[nodiscard]]
    rhi::PipelineProtocol* pipeline() const;

    /// Return a pointer to the RHI resource set (descriptors) for this
    /// material.
    [[nodiscard]]
    rhi::ResourceSetProtocol* resourceSet() const;

    /// Return a pointer to the texture bound to the specified 'slot' of this
    /// material, or an empty pointer if that slot holds none.
    [[nodiscard]]
    Texture* texture(TextureSlot slot) const;

    /// Return a pointer to the uniform buffer holding the factors of this
    /// material.
    [[nodiscard]]
    rhi::BufferProtocol* paramsUbo() const;

    /// Return the factors of this material.
    [[nodiscard]]
    const MaterialParams& params() const;

    /// Return the alpha mode of this material.
    [[nodiscard]]
    AlphaMode alphaMode() const;

    /// Return 'true' if this material is drawn on both of its faces, and
    /// 'false' otherwise.
    [[nodiscard]]
    bool doubleSided() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline rhi::PipelineProtocol* Material::pipeline() const
{
    return d_pipeline_p;
}

inline rhi::ResourceSetProtocol* Material::resourceSet() const
{
    return d_resourceSet_p;
}

inline Texture* Material::texture(TextureSlot slot) const
{
    return d_textures_p[static_cast<std::size_t>(slot)];
}

inline rhi::BufferProtocol* Material::paramsUbo() const
{
    return d_paramsUbo.get();
}

inline const MaterialParams& Material::params() const
{
    return d_params;
}

inline AlphaMode Material::alphaMode() const
{
    return d_alphaMode;
}

inline bool Material::doubleSided() const
{
    return d_doubleSided;
}

}  // close package namespace

#endif  // INCLUDED_ENG_RND_MATERIAL_H
