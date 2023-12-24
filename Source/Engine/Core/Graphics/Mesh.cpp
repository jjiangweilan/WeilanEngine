#include "Mesh.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/GLB.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include <filesystem>

DEFINE_ASSET(Mesh, "8D66F112-935C-47B1-B62F-728CBEA20CBD", "mesh");

Submesh::~Submesh() {}
Submesh::Submesh(
    std::unique_ptr<unsigned char>&& vertexBuffer,
    std::vector<VertexBinding>&& bindings,
    std::unique_ptr<unsigned char>&& indexBuffer,
    Gfx::IndexBufferType indexBufferType,
    int indexCount,
    std::string_view name
)
    : indexBufferType(indexBufferType), bindings(std::move(bindings)), indexCount(indexCount), name(name),
      vertexBuffer(std::move(vertexBuffer)), indexBuffer(std::move(indexBuffer))
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
    //
    // bufCreateInfo.size = indexBufferSize + vertexBufferSize;
    // bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    // bufCreateInfo.debugName = "mesh staging buffer";
    // bufCreateInfo.visibleInCPU = true;
    // auto stagingBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    GetGfxDriver()->UploadBuffer(*gfxVertexBuffer, vertexBuffer.get(), vertexBufferSize);
    GetGfxDriver()->UploadBuffer(*gfxIndexBuffer, indexBuffer.get(), indexBufferSize);

    // memcpy(stagingBuffer->GetCPUVisibleAddress(), this->vertexBuffer.get(), vertexBufferSize);
    // memcpy(
    //     (uint8_t*)stagingBuffer->GetCPUVisibleAddress() + vertexBufferSize,
    //     this->indexBuffer.get(),
    //     indexBufferSize
    // );
    //
    // ImmediateGfx::OnetimeSubmit(
    //     [this, vertexBufferSize, indexBufferSize, &stagingBuffer](Gfx::CommandBuffer& cmd)
    //     {
    //         Gfx::BufferCopyRegion bufferCopyRegions[] = {{0, 0, vertexBufferSize}};
    //         cmd.CopyBuffer(stagingBuffer, gfxVertexBuffer, bufferCopyRegions);
    //
    //         bufferCopyRegions[0] = {vertexBufferSize, 0, indexBufferSize};
    //         cmd.CopyBuffer(stagingBuffer, gfxIndexBuffer, bufferCopyRegions);
    //     }
    // );
};

void Submesh::SetIndices(std::vector<uint32_t>&& indices)
{
    this->indices = std::move(indices);
    indexCount = indices.size();
}

void Submesh::SetIndices(const std::vector<uint32_t>& indices)
{
    this->indices = indices;
    indexCount = indices.size();
}

void Submesh::SetPositions(std::vector<glm::vec3>&& positions)
{
    this->positions = std::move(positions);
}

void Submesh::SetPositions(const std::vector<glm::vec3>& positions)
{
    this->positions = positions;
}

void Submesh::SetVertexAttribute(VertexAttribute&& vertAttributes)
{
    this->attributes = std::move(vertAttributes);
}

void Submesh::SetVertexAttribute(const VertexAttribute& vertAttributes)
{
    this->attributes = vertAttributes;
}

void Submesh::Apply()
{
    bindings.clear();

    VertexBinding posBinding{
        .byteOffset = 0,
        .byteSize = positions.size() * sizeof(glm::vec3),
        .name = "position",
    };

    VertexBinding attrBinding{
        .byteOffset = posBinding.byteSize,
        .byteSize = attributes.GetSize(),
        .name = "attributes",
    };

    bindings.push_back(posBinding);
    bindings.push_back(attrBinding);

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
    size_t indexByteSize = indexBufferType == Gfx::IndexBufferType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    std::size_t indexBufferSize = indices.size() * indexByteSize;
    bufCreateInfo.size = indexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst;
    bufCreateInfo.debugName = name.data();
    gfxIndexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    // create staging buffer
    bufCreateInfo.size = indexBufferSize + vertexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    bufCreateInfo.debugName = "mesh staging buffer";
    bufCreateInfo.visibleInCPU = true;
    auto stagingBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    size_t positionDataSize = positions.size() * sizeof(glm::vec3);
    size_t attribtueDataSize = attributes.GetData().size();

    memcpy(stagingBuffer->GetCPUVisibleAddress(), positions.data(), positionDataSize);
    memcpy(
        (uint8_t*)stagingBuffer->GetCPUVisibleAddress() + positionDataSize,
        attributes.GetData().data(),
        attribtueDataSize
    );

    if (indexBufferType == Gfx::IndexBufferType::UInt16)
    {
        std::vector<uint16_t> temp(indices.size());
        const size_t indicesSize = indices.size();
        for (size_t i = 0; i < indicesSize; ++i)
        {
            temp[i] = indices[i];
        }
        memcpy(
            (uint8_t*)stagingBuffer->GetCPUVisibleAddress() + positionDataSize + attribtueDataSize,
            temp.data(),
            indicesSize * sizeof(uint16_t)
        );
    }
    else
    {
        memcpy(
            (uint8_t*)stagingBuffer->GetCPUVisibleAddress() + positionDataSize + attribtueDataSize,
            indices.data(),
            indices.size() * sizeof(uint32_t)
        );
    }
    indexCount = indices.size();

    ImmediateGfx::OnetimeSubmit(
        [this, vertexBufferSize, indexBufferSize, &stagingBuffer](Gfx::CommandBuffer& cmd)
        {
            Gfx::BufferCopyRegion bufferCopyRegions[] = {{0, 0, vertexBufferSize}};
            cmd.CopyBuffer(stagingBuffer, gfxVertexBuffer, bufferCopyRegions);

            bufferCopyRegions[0] = {vertexBufferSize, 0, indexBufferSize};
            cmd.CopyBuffer(stagingBuffer, gfxIndexBuffer, bufferCopyRegions);
        }
    );
}

const AABB& Submesh::GetAABB() const
{
    return aabb;
}

void Submesh::SetAABB(const AABB& aabb)
{
    this->aabb = aabb;
}

const std::vector<uint32_t>& Submesh::GetIndices() const
{
    return indices;
}

const std::vector<glm::vec3>& Submesh::GetPositions() const
{
    return positions;
}

const VertexAttribute& Submesh::GetAttribute() const
{
    return attributes;
}

bool Mesh::LoadFromFile(const char* path)
{
    std::vector<uint32_t> fullData;
    nlohmann::json jsonData;
    unsigned char* binaryData;
    Utils::GLB::GetGLBData(path, fullData, jsonData, binaryData);
    auto meshes = Utils::GLB::ExtractMeshes(jsonData, binaryData, 1);
    if (!meshes.empty())
    {
        submeshes = std::move(meshes[0]->submeshes);
        SetName(meshes[0]->GetName());
    }
    else
        return false;

    return true;
}

const AABB& Mesh::GetAABB() const
{
    return aabb;
}

Mesh::~Mesh() {}
