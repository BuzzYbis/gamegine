// texture.cpp                                                        -*-C++-*-
#include <rnd/texture.h>

// std
#include <filesystem>
#include <iostream>
#include <stdexcept>

// rhi
#include <rhi/rhi_types.h>

// third-party
#define STB_IMAGE_IMPLEMENTATION
#include <ktx.h>
#include <stb_image.h>

namespace eng::rnd {

// -------------
// class Texture
// -------------

// PRIVATE MANIPULATORS

void Texture::upload(rhi::ContextProtocol* context,
                     const unsigned char*  pixels,
                     const int             width,
                     const int             height,
                     const rhi::Format     format)
{
    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(
                                   std::log2(std::max(width, height)))) +
                               1;

    d_texture = context->createTexture(static_cast<uint32_t>(width),
                                       static_cast<uint32_t>(height),
                                       mipLevels,
                                       format,
                                       pixels);
}

// CREATORS
Texture::Texture(rhi::ContextProtocol* context,
                 const std::string&    texPath,
                 const rhi::Format     format)
{
    int texWidth    = 0;
    int texHeight   = 0;
    int texChannels = 0;

    const std::filesystem::path path(texPath);
    const std::string           extension = path.extension().string();

    if (extension == ".ktx2") {
        ktxTexture2*   kTexture;
        KTX_error_code result = ktxTexture2_CreateFromNamedFile(
            texPath.c_str(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &kTexture);

        if (result != KTX_SUCCESS) {
            throw std::runtime_error("failed to load ktx texture image!");
        }

        if (ktxTexture2_NeedsTranscoding(kTexture)) {
            result = ktxTexture2_TranscodeBasis(kTexture, KTX_TTF_RGBA32, 0);
            if (result != KTX_SUCCESS) {
                ktxTexture_Destroy(ktxTexture(kTexture));
                throw std::runtime_error("failed to transcode ktx texture!");
            }
        }

        texWidth  = kTexture->baseWidth;
        texHeight = kTexture->baseHeight;

        // Level 0 is not necessarily at offset 0 inside the data blob.
        ktx_size_t offset = 0;
        ktxTexture_GetImageOffset(ktxTexture(kTexture), 0, 0, 0, &offset);

        upload(context,
               ktxTexture_GetData(ktxTexture(kTexture)) + offset,
               texWidth,
               texHeight,
               format);
        ktxTexture_Destroy(ktxTexture(kTexture));
    }
    else {
        stbi_uc* pixels = stbi_load(texPath.c_str(),
                                    &texWidth,
                                    &texHeight,
                                    &texChannels,
                                    STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image: " +
                                     texPath);
        }

        upload(context, pixels, texWidth, texHeight, format);
        stbi_image_free(pixels);
    }
}

Texture::Texture(rhi::ContextProtocol*          context,
                 std::span<const unsigned char> encoded,
                 const std::string&             label,
                 const rhi::Format              format)
{
    if (encoded.empty()) {
        throw std::runtime_error("Empty texture image: " + label);
    }

    int texWidth    = 0;
    int texHeight   = 0;
    int texCahnnels = 0;

    stbi_uc* pixels = stbi_load_from_memory(encoded.data(),
                                            static_cast<int>(encoded.size()),
                                            &texWidth,
                                            &texHeight,
                                            &texCahnnels,
                                            STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("Failed to decode texture image: " + label +
                                 " (" + stbi_failure_reason() + ")");
    }

    upload(context, pixels, texWidth, texHeight, format);

    stbi_image_free(pixels);
}

Texture::Texture(rhi::ContextProtocol*                context,
                 const std::span<const unsigned char> pixels,
                 const int                            width,
                 const int                            height,
                 const rhi::Format                    format)
{
    const std::size_t expected = static_cast<std::size_t>(width) *
                                 static_cast<std::size_t>(height) * 4;

    if (width <= 0 || height <= 0 || pixels.size() != expected) {
        throw std::runtime_error("Texture pixel span does not match its "
                                 "declared dimensions");
    }

    upload(context, pixels.data(), width, height, format);
}

}  // close package namespace
