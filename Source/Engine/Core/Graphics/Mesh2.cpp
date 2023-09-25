#include "Mesh2.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ImmediateGfx.hpp"

namespace Engine
{

// Mesh2 should be imported through model that's why we just use a Mesh2PlaceHolder
DEFINE_ASSET(Mesh2, "8D66F112-935C-47B1-B62F-728CBEA20CBD", "Mesh2PlaceHolder");

Submesh::~Submesh() {}
Submesh::Submesh(
    std::unique_ptr<unsigned char>&& vertexBuffer,
    std::vector<VertexBinding>&& bindings,
    std::unique_ptr<unsigned char>&& indexBuffer,
    Gfx::IndexBufferType indexBufferType,
    int indexCount,
    std::string_view name
)
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

    bufCreateInfo.size = indexBufferSize + vertexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    bufCreateInfo.debugName = "mesh staging buffer";
    bufCreateInfo.visibleInCPU = true;
    auto stagingBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    memcpy(stagingBuffer->GetCPUVisibleAddress(), this->vertexBuffer.get(), vertexBufferSize);
    memcpy(
        (uint8_t*)stagingBuffer->GetCPUVisibleAddress() + vertexBufferSize,
        this->indexBuffer.get(),
        indexBufferSize
    );

    ImmediateGfx::OnetimeSubmit(
        [this, vertexBufferSize, indexBufferSize, &stagingBuffer](Gfx::CommandBuffer& cmd)
        {
            Gfx::BufferCopyRegion bufferCopyRegions[] = {{0, 0, vertexBufferSize}};
            cmd.CopyBuffer(stagingBuffer, gfxVertexBuffer, bufferCopyRegions);

            bufferCopyRegions[0] = {vertexBufferSize, 0, indexBufferSize};
            cmd.CopyBuffer(stagingBuffer, gfxIndexBuffer, bufferCopyRegions);
        }
    );
};

Mesh2::~Mesh2() {}

} // namespace Engine
