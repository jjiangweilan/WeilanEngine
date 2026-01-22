#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

#include "Engine/Library/ObjectPool.hpp"
#include "Mesh.hpp"
#include "RenderCoreData.hpp"

#include <cinttypes>
#include <vk_mem_alloc.h> // for virtual memory allocator

namespace RenderCoreModule
{
class MeshManager
{
public:
    MeshManager();

    ~MeshManager();

    MeshHandleIndex CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16);
    void DestroyMesh(MeshHandleIndex handle);

    Mesh* GetMesh(MeshHandleIndex handle);
    Gfx::Buffer* GetMeshBuffer();

private:
    Spinlock vmaLock;
    ObjectPool<Mesh> meshes;
    VmaVirtualBlock meshBufferBlock;
    std::unique_ptr<Gfx::Buffer> meshBuffer;
    RenderCore* rc;
};
} // namespace RenderCoreModule
