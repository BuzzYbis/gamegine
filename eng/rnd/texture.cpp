// texture.cpp                                                        -*-C++-*-
#include <rnd/texture.h>

// std
#include <iostream>
#include <stdexcept>

// rhi
#include <rhi/rhi_types.h>

// third-party
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace eng::rnd {

// -------------
// class Texture
// -------------

// CREATORS
Texture::Texture(rhi::ContextProtocol* context, const std::string& texPath)
{
    int      texWidth    = 0;
    int      texHeight   = 0;
    int      texChannels = 0;
    stbi_uc* pixels      = stbi_load(texPath.c_str(),
                                &texWidth,
                                &texHeight,
                                &texChannels,
                                STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("failed to load texture image: " + texPath);
    }

    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(
                                   std::log2(std::max(texWidth, texHeight)))) +
                               1;

    d_texture = context->createTexture(static_cast<uint32_t>(texWidth),
                                       static_cast<uint32_t>(texHeight),
                                       mipLevels,
                                       rhi::Format::R8G8B8A8_SRGB,
                                       pixels);

    stbi_image_free(pixels);
}

}  // close package namespace
