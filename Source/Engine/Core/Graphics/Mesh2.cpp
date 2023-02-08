#include "Mesh2.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/GfxResourceTransfer.hpp"

namespace Engine
{
Submesh::Submesh(UniPtr<unsigned char>&& vertexBuffer,
                 std::vector<VertexBinding>&& bindings,
                 UniPtr<unsigned char>&& indexBuffer,
                 Gfx::IndexBufferType indexBufferType,
                 int indexCount,
                 std::string_view name)
    : vertexBuffer(std::move(vertexBuffer)), indexBuffer(std::move(indexBuffer)), indexBufferType(indexBufferType),
      bindings(std::move(bindings)), indexCount(indexCount), name(name)
{
    // calculate vertex buffer size
    std::size_t vertexBufferSize = 0;
    for (auto& binding : this->bindings)
    {
        vertexBufferSize += binding.byteSize;
    }

    // create vertex buffer
    Gfx::Buffer::CreateInfo bufCreateInfo;
    bufCreateInfo.size = vertexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Transfer_Dst;
    bufCreateInfo.debugName = name.data();
    gfxVertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    // calculate index buffer size
    std::size_t indexBufferSize = indexCount * (indexBufferType == Gfx::IndexBufferType::UInt16 ? 2 : 4);
    bufCreateInfo.size = indexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst;
    bufCreateInfo.debugName = name.data();
    gfxIndexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    // upload vertex buffer
    Internal::GfxResourceTransfer::BufferTransferRequest request0{.data = this->vertexBuffer.Get(),
                                                                  .bufOffset = 0,
                                                                  .size = vertexBufferSize};
    Internal::GetGfxResourceTransfer()->Transfer(gfxVertexBuffer.Get(), request0);

    // upload index buffer
    Internal::GfxResourceTransfer::BufferTransferRequest request1{
        .data = this->indexBuffer.Get(),
        .bufOffset = 0,
        .size = indexBufferSize,
    };
    Internal::GetGfxResourceTransfer()->Transfer(gfxIndexBuffer, request1);
};
} // namespace Engine
