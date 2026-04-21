// material.cpp                                                       -*-C++-*-
#include <rnd/material.h>

namespace eng::rnd {

// --------------
// class Material
// --------------

// MANIPULATORS
void Material::setPipeline(rhi::PipelineProtocol* pipeline)
{
    d_pipeline_p = pipeline;
}

void Material::setResourceSet(rhi::ResourceSetProtocol* set)
{
    d_resourceSet_p = set;
}

void Material::setTexture(Texture* texture)
{
    d_texture_p = texture;
}

}  // close package namespace
