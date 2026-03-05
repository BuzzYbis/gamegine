#pragma once
#include <array>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.hpp>

namespace engine::core {
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    bool operator==(const Vertex& other) const
    {
        return position == other.position && normal == other.normal &&
               uv == other.uv;
    }

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 3>
    getAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription(0,
                                                0,
                                                vk::Format::eR32G32B32Sfloat,
                                                offsetof(Vertex, position)),
            vk::VertexInputAttributeDescription(1,
                                                0,
                                                vk::Format::eR32G32B32Sfloat,
                                                offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription(2,
                                                0,
                                                vk::Format::eR32G32Sfloat,
                                                offsetof(Vertex, uv))};
    }
};
}  // namespace core

namespace std {
template <>
struct hash<engine::core::Vertex> {
    size_t operator()(engine::core::Vertex const& vertex) const noexcept
    {
        return (hash<glm::vec3>()(vertex.position) ^
                hash<glm::vec3>()(vertex.normal) << 1) >>
                   1 ^
               hash<glm::vec2>()(vertex.uv) << 1;
    }
};
}  // namespace std
