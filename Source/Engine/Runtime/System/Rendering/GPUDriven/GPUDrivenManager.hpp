#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Library/Allocators/VirtualTLSFAllocator.hpp"
#include "Engine/Library/ObjectPool.hpp"

class Submesh;
namespace Rendering
{

using GPUMeshHandle = uint64_t;

class GPUDrivenManager
{
public:
    struct SceneObjectVertexDataDescriptor
    {
        VirtualTLSFAllocator::Allocation dataAlloc;
        uint32_t indexOffset = 0;
        uint32_t positionOffset = 0;
        uint32_t attributeOffset = 0;
    };

    static GPUDrivenManager& Instance();

    GPUMeshHandle RegisterMesh(const Submesh& submesh);
    void UnregisterMesh(GPUMeshHandle handle);

    const SceneObjectVertexDataDescriptor& GetSceneObjectVertexDataDescriptor(GPUMeshHandle handle) { return sceneObjectVertexDataDescriptors[handle]; }

    Gfx::Buffer* GetGlobalBuffer() { return globalBuffer.get(); }

    void Deinit();

private:
    GPUDrivenManager();

    const uint8_t globalDataAlignment = 8;
    uint32_t globalBufferSize = 512 * 1024 * 1024;
    VirtualTLSFAllocator globalBufferAllocator{globalBufferSize}; // 512MB for all data
    std::unique_ptr<Gfx::Buffer> globalBuffer;
    ObjectPool<SceneObjectVertexDataDescriptor> sceneObjectVertexDataDescriptors;

    void AllocateForMesh(SceneObjectVertexDataDescriptor& descriptor, const Submesh& submesh);

    static std::unique_ptr<GPUDrivenManager>& GetInstanceInternal();
};
} // namespace Rendering
