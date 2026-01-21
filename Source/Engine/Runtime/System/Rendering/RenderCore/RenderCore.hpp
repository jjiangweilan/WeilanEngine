#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/CommandStream.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Library/SpinLock.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline1/RenderPipeline1.hpp"
#include "Engine/Runtime/System/Rendering/RenderScene.hpp"
#include "Mesh.hpp"
#include "RenderCoreData.hpp"
#include <span>
#include <vk_mem_alloc.h> // for virtual memory allocator

class Scene;
class Camera;

namespace RenderCoreModule
{
class RenderCore;

using RenderCoreCommandStream = CommandStream;

class RenderCore;

class RC_CMC : public CommandStreamContext
{
public:
    RenderCore* rc;
};

struct UploadMeshDataCmd
{
    MeshHandle handle;
    uint8_t* vertexData;
    uint8_t* indexData;
    uint32_t vertexDataSize;
    uint32_t indexDataSize;

    static void Execute(CommandStreamContext* context, void* ptr);
};
class RenderCore
{
public:
    MeshHandle CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16)
    {
        return MeshHandle(
            this,
            meshManager.CreateMesh(vertexByteSize, indexCount, indexBit_16)
        );
    }

    void DestroyMesh(MeshHandle handle)
    {
        meshManager.DestroyMesh(handle.GetHandleIndex());
    }

    void UploadMeshData(MeshHandleIndex& handleIndex, std::span<uint8_t> vertexData, std::span<uint8_t> indexData);

    void FlushCommands()
    {
    }

    void Render(RenderScene& scene, std::span<Camera*> cameras, RenderPipeline1 pipelineHandle);

    Gfx::Buffer* GetMeshBuffer()
    {
        return meshManager.GetMeshBuffer();
    }

private:
    struct MeshManager
    {
        MeshManager()
        {
            meshBuffer = GetGfxDriver()->CreateBuffer(1024 * 1024 * 64, Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Storage, false, false, "RenderCore-MeshBuffer");
            auto virtualBlockCreateInfo = VmaVirtualBlockCreateInfo{
                .size = meshBuffer->GetSize(),
                .flags = VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT,
            };

            vmaCreateVirtualBlock(&virtualBlockCreateInfo, &meshBufferBlock);
        }

        ~MeshManager()
        {
            meshBuffer = nullptr;
            vmaDestroyVirtualBlock(meshBufferBlock);
        }

        void UploadMeshData(MeshHandle& handle, std::span<uint8_t> vertexData, std::span<uint8_t> indexData)
        {
        }

        MeshHandleIndex CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16)
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

        void DestroyMesh(MeshHandleIndex handle)
        {
            auto& mesh = meshes[handle];

            ScopedSpinLock lock(vmaLock);
            vmaVirtualFree(meshBufferBlock, mesh.vertexHandle);

            meshes.Free(handle);
        }

        Gfx::Buffer* GetMeshBuffer() { return meshBuffer.get(); }

        Spinlock vmaLock;
        ObjectPool<Mesh> meshes;
        VmaVirtualBlock meshBufferBlock;
        std::unique_ptr<Gfx::Buffer> meshBuffer;
        RenderCore* rc;
    };

    RenderCoreCommandStream cm;
    MeshManager meshManager;
    std::vector<std::unique_ptr<RenderPipeline1>> renderPipelines;
};

void UploadMeshDataCmd::Execute(CommandStreamContext* context, void* ptr)
{
    RC_CMC* rcContext = static_cast<RC_CMC*>(context);
    RenderCore* rc = rcContext->rc;
    UploadMeshDataCmd* cmd = (UploadMeshDataCmd*)ptr;
    Gfx::Buffer* meshBuffer = rc->GetMeshBuffer();

    MeshHandle& handle = cmd->handle;

    GetGfxDriver()->UploadBuffer(*meshBuffer, cmd->indexData, cmd->indexDataSize, handle.indexOffset);
    GetGfxDriver()->UploadBuffer(*meshBuffer, cmd->vertexData, cmd->vertexDataSize, handle.vertexOffset);
}

void RenderCore::UploadMeshData(MeshHandle& handle, std::span<uint8_t> vertexData, std::span<uint8_t> indexData)
{
    meshManager.UploadMeshData(handle, vertexData, indexData);

    void* rawData = nullptr;
    UploadMeshDataCmd* cmd = cm.Push<UploadMeshDataCmd>(
        &UploadMeshDataCmd::Execute,
        &rawData,
        vertexData.size() + indexData.size()
    );

    cmd->handle = handle;
    cmd->vertexData = (uint8_t*)rawData;
    cmd->vertexDataSize = static_cast<uint32_t>(vertexData.size());
    memcpy(cmd->vertexData, vertexData.data(), vertexData.size());

    cmd->indexData = cmd->vertexData + vertexData.size();
    cmd->indexDataSize = static_cast<uint32_t>(indexData.size());
    memcpy(cmd->indexData, indexData.data(), indexData.size());
}

void RenderCore::Render(RenderScene& scene, std::span<Camera*> cameras, RenderPipeline1 pipelineHandle)
{
}

} // namespace RenderCoreModule
