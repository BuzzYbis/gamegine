// material.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_RND_MATERIAL_H
#define INCLUDED_ENG_RND_MATERIAL_H

//@PURPOSE: Provide a high-level representation of a surface material.
//
//@CLASSES:
//  eng::rnd::Material: Encapsulates shader state and texture resources.
//
//@DESCRIPTION: This component provides a high-level 'Material' class that
// links a graphics pipeline (shaders, state) with specific resource sets
// (textures, uniforms) required to render a mesh.

// renderer
#include <rnd/texture.h>

namespace eng::rnd {

// ==============
// class Material
// ==============

/// High-level representation of a surface material.
class Material {
  private:
    // DATA
    rhi::PipelineProtocol*    d_pipeline_p;
    rhi::ResourceSetProtocol* d_resourceSet_p;
    Texture*                  d_texture_p;

  public:
    // CREATORS

    /// Create a material.
    Material() = default;

    /// Destroy this material.
    ~Material() = default;

    // MANIPULATORS

    /// Set the graphics pipeline for this material to the specified
    /// 'pipeline'.
    void setPipeline(rhi::PipelineProtocol* pipeline);

    /// Set the resource set (uniforms, textures) for this material to the
    /// specified 'set'.
    void setResourceSet(rhi::ResourceSetProtocol* set);

    /// Set the high-level texture for this material to the specified
    /// 'texture'.
    void setTexture(Texture* texture);

    // ACCESSORS

    /// Return a pointer to the graphics pipeline associated with this
    /// material.
    [[nodiscard]]
    rhi::PipelineProtocol* pipeline() const;

    /// Return a pointer to the RHI resource set (descriptors) for this
    /// material.
    [[nodiscard]]
    rhi::ResourceSetProtocol* resourceSet() const;

    /// Return a pointer to the texture used by this material.
    [[nodiscard]]
    Texture* texture() const;
};

// =======================================================================
//                          INLINE DEFINITIONS
// =======================================================================

// ACCESSORS

inline rhi::PipelineProtocol* Material::pipeline() const
{
    return d_pipeline_p;
}

inline Texture* Material::texture() const
{
    return d_texture_p;
}

inline rhi::ResourceSetProtocol* Material::resourceSet() const
{
    return d_resourceSet_p;
}

}  // close package namespace

#endif  // INCLUDED_ENG_RND_MATERIAL_H
