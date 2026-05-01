#include "Mesh.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include <filesystem>

DEFINE_ASSET(Mesh, "8D66F112-935C-47B1-B62F-728CBEA20CBD", "mesh");

Submesh::~Submesh() {}

uint32_t Submesh::GetVertexDataByteSize() const
{
    return vertexBufferSize;
}

uint32_t Submesh::GetIndexDataByteSize() const
{
    return indexBufferSize;
}

void Submesh::UpdateVertexDataByteSize()
{
    vertexBufferSize = 0;
    for (auto& binding : this->bindings)
    {
        vertexBufferSize += binding.byteSize;
    }
}

void Submesh::UpdateIndexDataByteSize()
{
    indexBufferSize = indexCount * (indexBufferType == Gfx::IndexBufferType::UInt16 ? 2 : 4);
}

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

void Submesh::SetVertexAttribute(VertexAttributes&& vertAttributes)
{
    this->attributes = std::move(vertAttributes);
}

void Submesh::SetVertexAttribute(const VertexAttributes& vertAttributes)
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
    indexCount = indices.size();

    UpdateVertexDataByteSize();
    UpdateIndexDataByteSize();

    // create vertex buffer
    Gfx::Buffer::CreateInfo bufCreateInfo;
    bufCreateInfo.size = vertexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::ShaderDeviceAddress;
    bufCreateInfo.debugName = name.data();
    gfxVertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);
    VertexAttributes positionBinding{};
    positionBinding
        .AddAttribute("position", VertexAttributeSemantics::Position, 0, sizeof(decltype(positions)::value_type));
    gfxVertexBuffer->SetVertexAttributes(0, positionBinding);
    gfxVertexBuffer->SetVertexAttributes(1, attributes);

    // calculate index buffer size
    size_t indexByteSize = indexBufferType == Gfx::IndexBufferType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    std::size_t indexBufferSize = indices.size() * indexByteSize;
    bufCreateInfo.size = indexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::ShaderDeviceAddress;
    bufCreateInfo.debugName = name.data();
    gfxIndexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    // create staging buffer
    bufCreateInfo.size = indexBufferSize + vertexBufferSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    bufCreateInfo.debugName = "mesh staging buffer";
    bufCreateInfo.visibleInCPU = true;
    uint8_t* staging = new uint8_t[indexBufferSize + vertexBufferSize];

    size_t positionDataSize = positions.size() * sizeof(glm::vec3);
    size_t attribtueDataSize = attributes.GetData().size();

    memcpy(staging, positions.data(), positionDataSize);
    memcpy(staging + positionDataSize, attributes.GetData().data(), attribtueDataSize);

    if (indexBufferType == Gfx::IndexBufferType::UInt16)
    {
        std::vector<uint16_t> temp(indices.size());
        const size_t indicesSize = indices.size();
        for (size_t i = 0; i < indicesSize; ++i)
        {
            temp[i] = indices[i];
        }
        memcpy(staging + positionDataSize + attribtueDataSize, temp.data(), indicesSize * sizeof(uint16_t));
    }
    else
    {
        memcpy(staging + positionDataSize + attribtueDataSize, indices.data(), indices.size() * sizeof(uint32_t));
    }
    indexCount = indices.size();

    GetGfxDriver()->UploadBuffer(*gfxVertexBuffer, staging, vertexBufferSize, 0);
    GetGfxDriver()->UploadBuffer(*gfxIndexBuffer, staging + vertexBufferSize, indexBufferSize, 0);
    delete[] staging;

    // generate gfx vertex binding
    for (auto& b : bindings)
    {
        gfxBindings.push_back({GetVertexBuffer(), b.byteOffset});
    };

    gpuMeshHandle = Rendering::GPUDrivenManager::Instance().RegisterGeometry(*this);
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

const VertexAttributes& Submesh::GetAttribute() const
{
    return attributes;
}

bool Mesh::LoadFromFile(const char* path)
{
    return false;
}

const AABB& Mesh::GetAABB() const
{
    return aabb;
}

Rendering::GpuGeometryHandle Submesh::GetGPUGeometryHandle() const
{
    ASSERT_IS_MAIN_THREAD

    if (gpuMeshHandle == -1)
    {
        gpuMeshHandle = Rendering::GPUDrivenManager::Instance().RegisterGeometry(*this);
    }

    return gpuMeshHandle;
}

uint32_t Submesh::GetGPUMeshPositionOffset() const
{
    return Rendering::GPUDrivenManager::Instance().GetGeometryDescriptor(GetGPUGeometryHandle()).geometry.positionOffset;
}

uint32_t Submesh::GetGPUMeshIndexOffset() const
{
    return Rendering::GPUDrivenManager::Instance().GetGeometryDescriptor(GetGPUGeometryHandle()).geometry.indexOffset;
}

uint32_t Submesh::GetGPUMeshAttributeOffset() const
{
    return Rendering::GPUDrivenManager::Instance().GetGeometryDescriptor(GetGPUGeometryHandle()).geometry.attributeOffset;
}

uint64_t Submesh::GetVertexBufferShaderDeviceAddress() const
{
    return Rendering::GPUDrivenManager::Instance().GetGlobalBufferShaderDeviceAddress() + GetGPUMeshPositionOffset();
}

uint64_t Submesh::GetIndexBufferShaderDeviceAddress() const
{
    return Rendering::GPUDrivenManager::Instance().GetGlobalBufferShaderDeviceAddress() + GetGPUMeshIndexOffset();
}

Mesh::~Mesh() {}
