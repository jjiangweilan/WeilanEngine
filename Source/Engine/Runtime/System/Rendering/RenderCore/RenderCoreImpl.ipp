#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/SpinLock.hpp"
#include "RenderCoreData.hpp"
#include <span>
#include <vk_mem_alloc.h> // for virtual memory allocator

class Scene;
class Camera;
class RenderCoreImpl;

class RenderCoreImpl
{
public:
    MeshHandle CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16)
    {
        return meshManager.CreateMesh(vertexByteSize, indexCount, indexBit_16);
    }

    void DestroyMesh(MeshHandle handle)
    {
        meshManager.DestroyMesh(handle);
    }

    void UploadMeshData(MeshHandle& handle, std::span<uint8_t> vertexData, std::span<uint8_t> indexData)
    {
        meshManager.UploadMeshData(handle, vertexData, indexData);
    }

    void RenderScene(Scene* scene, std::span<Camera*> camera);

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

        MeshHandle CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16)
        {
            VmaVirtualAllocationCreateInfo vertexAllocCreateInfo = {};
            vertexAllocCreateInfo.size = vertexByteSize;
            vertexAllocCreateInfo.alignment = 4; // alignmned for float_X
            vertexAllocCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

            VmaVirtualAllocationCreateInfo indexAllocCreateInfo = {};
            indexAllocCreateInfo.size = indexCount * (indexBit_16 ? 2 : 4);
            indexAllocCreateInfo.alignment = indexBit_16 ? 2 : 4;
            indexAllocCreateInfo.flags = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

            MeshHandle handle{};

            {
                ScopedSpinLock lock(vmaLock);
                VmaVirtualAllocation& vertexVirtualAlloc = handle.vertexHandle;
                VkDeviceSize& vertexOffset = handle.vertexOffset;
                if (vmaVirtualAllocate(meshBufferBlock, &vertexAllocCreateInfo, &vertexVirtualAlloc, &vertexOffset) != VK_SUCCESS)
                {
                    spdlog::error("RenderCore: Failed to allocate vertex buffer sub allocation");
                }

                VmaVirtualAllocation& indexVirtualAlloc = handle.indexHandle;
                VkDeviceSize& indexOffset = handle.indexOffset;
                if (vmaVirtualAllocate(meshBufferBlock, &indexAllocCreateInfo, &indexVirtualAlloc, &indexOffset))
                {
                    spdlog::error("RenderCore: Failed to allocate index buffer sub allocation");
                }
            }

            return handle;
        }

        void DestroyMesh(MeshHandle handle)
        {
            ScopedSpinLock lock(vmaLock);
            vmaVirtualFree(meshBufferBlock, handle.vertexHandle);
        }

        Spinlock vmaLock;
        VmaVirtualBlock meshBufferBlock;
        std::unique_ptr<Gfx::Buffer> meshBuffer;
        RenderCoreImpl* rc;
    };

    MeshManager meshManager;
};
