#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "vk_mem_alloc.h" // for vma virtual memory allocator
class SceneMeshManager;

struct SceneMeshHandle
{
    VkDeviceSize vertexBufferOffset = 0;
    VkDeviceSize indexBufferOffset = 0;

    VmaVirtualAllocation _vertexAlloc;
    VmaVirtualAllocation _indexAlloc;
};

class SceneMesh
{
public:
    SceneMesh(SceneMeshManager* manager);

    void SetCPUMesh(Mesh* mesh);

    Gfx::Buffer* GetVertexBuffer();
    Gfx::Buffer* GetIndexBuffer();

private:
    ObjPtr<Mesh> cpuMesh;
    SceneMeshManager* manager;
};

class SceneMeshManager
{
public:
    SceneMeshManager();
    void Init(uint32_t totalVertexBufferSize, uint32_t totalIndexBufferSize);
    bool AllocateMesh(size_t size, SceneMeshHandle& outHandle);
    void FreeMesh(SceneMeshHandle& handle);
    Gfx::Buffer* GetGlobalVertexBuffer() const { return globalVertexBuffer.get(); }
    Gfx::Buffer* GetGlobalIndexBuffer() const { return globalIndexBuffer.get(); }

private:
    std::unique_ptr<Gfx::Buffer> globalVertexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> globalIndexBuffer = nullptr;

    VmaVirtualBlock virtualVertexBlock;
    VmaVirtualBlock virtualIndexBlock;
};
