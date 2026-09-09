// mesh.cpp                                                           -*-C++-*-
#include <rnd/mesh.h>

namespace eng::rnd {

Mesh::Mesh(rhi::ContextProtocol*            context,
           const std::vector<core::Vertex>& vertices,
           const std::vector<uint32_t>&     indices)
: d_indexCount(static_cast<uint32_t>(indices.size()))
{
    // 1. Create and Upload the Vertex Buffer
    const size_t vertexSize = vertices.size() * sizeof(core::Vertex);
    d_vertexBuffer          = context->createBuffer(vertexSize,
                                                    rhi::BufferUsage::Vertex);
    d_vertexBuffer->uploadData(vertices.data(), vertexSize, 0);

    // 2. Create and Upload the Index Buffer
    const size_t indexSize = indices.size() * sizeof(uint32_t);
    d_indexBuffer = context->createBuffer(indexSize, rhi::BufferUsage::Index);
    d_indexBuffer->uploadData(indices.data(), indexSize, 0);
}

void Mesh::draw(rhi::CommandListProtocol* cmd, uint32_t instanceCount) const
{
    // Bind geometry buffers
    cmd->bindVertexBuffer(d_vertexBuffer.get(), 0, 0);
    cmd->bindIndexBuffer(d_indexBuffer.get(), 0);

    // Indexed draw call: (indexCount, instanceCount, firstIndex, vertexOffset,
    // firstInstance)
    cmd->drawIndexed(d_indexCount, instanceCount, 0, 0, 0);
}

}  // close package namespace