// material.cpp                                                       -*-C++-*-
#include <rnd/material.h>

// std
#include <cstddef>

namespace eng::rnd {

// --------------
// class Material
// --------------

// CREATORS

Material::Material(rhi::ContextProtocol* context)
: d_paramsUbo(
      context->createBuffer(sizeof(MaterialParams), rhi::BufferUsage::Uniform))
{
    // The device copy must agree with 'd_params' from the start, so that a
    // material left with its neutral factors still reads as neutral in the
    // shader.
    d_paramsUbo->uploadData(&d_params, sizeof(MaterialParams), 0);
}

// MANIPULATORS

void Material::setPipeline(rhi::PipelineProtocol* pipeline)
{
    d_pipeline_p = pipeline;
}

void Material::setResourceSet(rhi::ResourceSetProtocol* set)
{
    d_resourceSet_p = set;
}

void Material::setTexture(TextureSlot slot, Texture* texture)
{
    d_textures_p[static_cast<std::size_t>(slot)] = texture;
}

void Material::setParams(const MaterialParams& params)
{
    d_params = params;
    d_paramsUbo->uploadData(&d_params, sizeof(MaterialParams), 0);
}

void Material::setAlphaMode(AlphaMode mode)
{
    d_alphaMode = mode;
}

void Material::setDoubleSided(bool doubleSided)
{
    d_doubleSided = doubleSided;
}

}  // close package namespace
