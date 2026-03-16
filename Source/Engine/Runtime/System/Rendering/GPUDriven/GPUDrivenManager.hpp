#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Library/Allocators/VirtualTLSFAllocator.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include <vk_mem_alloc.h>

namespace Rendering
{

using GPUMeshHandle = uint64_t;

class GPUDrivenManager
{
public:
    GPUMeshHandle RegisterMesh(Submesh& submesh);
    void UnregisterMesh(GPUMeshHandle handle);

    Gfx::Buffer* GetGlobalBuffer() { return globalBuffer.get(); }

private:
    struct SceneObjectVertexDataDescriptor
    {
        VirtualTLSFAllocator::Allocation dataAlloc;
        // index is always at first
        static const uint32_t indexOffset = 0;
        uint32_t positionOffset = 0;
        uint32_t vertexOffset;
    };

    const uint8_t globalDataAlignment = 8;
    VirtualTLSFAllocator globalBufferAllocator{512 * 1024 * 1024}; // 512MB for all data
    std::unique_ptr<Gfx::Buffer> globalBuffer;
    ObjectPool<SceneObjectVertexDataDescriptor> sceneObjectVertexDataDescriptors;

    void AllocateForMesh(SceneObjectVertexDataDescriptor& descriptor, Submesh& submesh);
};
} // namespace Rendering
