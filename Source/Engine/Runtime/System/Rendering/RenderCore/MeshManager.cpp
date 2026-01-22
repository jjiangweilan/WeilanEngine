#include "MeshManager.hpp"
#include <spdlog/spdlog.h>

namespace RenderCoreModule
{
MeshManager::MeshManager()
{
    meshBuffer = GetGfxDriver()->CreateBuffer(1024 * 1024 * 64, Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Storage, false, false, "RenderCore-MeshBuffer");
    auto virtualBlockCreateInfo = VmaVirtualBlockCreateInfo{
        .size = meshBuffer->GetSize(),
        .flags = VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT,
    };

    vmaCreateVirtualBlock(&virtualBlockCreateInfo, &meshBufferBlock);
}

MeshManager::~MeshManager()
{
    meshBuffer = nullptr;
    vmaDestroyVirtualBlock(meshBufferBlock);
}

MeshHandleIndex MeshManager::CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16)
{
    VmaVirtualAllocationCreateInfo vertexAllocCreateInfo = {};
    vertexAllocCreateInfo.size = vertexByteSize;
    vertexAllocCreateInfo.alignment = 4; // alignmned for float_X
    vertexAllocCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    VmaVirtualAllocationCreateInfo indexAllocCreateInfo = {};
    indexAllocCreateInfo.size = indexCount * (indexBit_16 ? 2 : 4);
    indexAllocCreateInfo.alignment = indexBit_16 ? 2 : 4;
    indexAllocCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    MeshHandleIndex handleIndex = meshes.Allocate();
    auto& mesh = meshes[handleIndex];

    {
        ScopedSpinLock lock(vmaLock);
        VmaVirtualAllocation& vertexVirtualAlloc = mesh.vertexHandle;
        VkDeviceSize& vertexOffset = mesh.vertexOffset;
        if (vmaVirtualAllocate(meshBufferBlock, &vertexAllocCreateInfo, &vertexVirtualAlloc, &vertexOffset) != VK_SUCCESS)
        {
            spdlog::error("RenderCore: Failed to allocate vertex buffer sub allocation");
        }

        VmaVirtualAllocation& indexVirtualAlloc = mesh.indexHandle;
        VkDeviceSize& indexOffset = mesh.indexOffset;
        if (vmaVirtualAllocate(meshBufferBlock, &indexAllocCreateInfo, &indexVirtualAlloc, &indexOffset))
        {
            spdlog::error("RenderCore: Failed to allocate index buffer sub allocation");
        }
    }

    return handleIndex;
}

void MeshManager::DestroyMesh(MeshHandleIndex handle)
{
    auto& mesh = meshes[handle];

    ScopedSpinLock lock(vmaLock);
    vmaVirtualFree(meshBufferBlock, mesh.vertexHandle);

    meshes.Free(handle);
}

Mesh* MeshManager::GetMesh(MeshHandleIndex handle)
{
    return &meshes[handle];
}

Gfx::Buffer* MeshManager::GetMeshBuffer()
{
    return meshBuffer.get();
}
} // namespace RenderCoreModule
