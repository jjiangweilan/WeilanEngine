#include "GPUDrivenManager.hpp"
#include "Engine/Library/Allocators/ThreadLocalAllocator.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"

namespace Rendering
{
GPUMeshHandle GPUDrivenManager::RegisterMesh(const Submesh& submesh)
{
    GPUMeshHandle handle = sceneObjectVertexDataDescriptors.AllocateRaw();
    SceneObjectVertexDataDescriptor& newDescriptor = sceneObjectVertexDataDescriptors[handle];
    AllocateForMesh(newDescriptor, submesh);

    return handle;
}

GPUDrivenManager::GPUDrivenManager()
{
    globalBuffer = GetGfxDriver()->CreateBuffer(
        globalBufferSize,
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        false,
        true,
        "GPUDrivenGlobalBuffer"
    );
}

void GPUDrivenManager::UnregisterMesh(GPUMeshHandle handle)
{
    SceneObjectVertexDataDescriptor& descriptor = sceneObjectVertexDataDescriptors[handle];
    globalBufferAllocator.Free(descriptor.dataAlloc);
    sceneObjectVertexDataDescriptors.FreeRaw(static_cast<int>(handle));
}

void GPUDrivenManager::AllocateForMesh(SceneObjectVertexDataDescriptor& descriptor, const Submesh& submesh)
{
    auto vertexByteSize = submesh.GetVertexDataByteSize();
    auto indexByteSize = submesh.GetIndexDataByteSize();

    ThreadLocalAllocator tempAllocator;

    auto totalSize = vertexByteSize + indexByteSize;

    // allocate CPU temp buffer
    uint8_t* staging = (uint8_t*)tempAllocator.allocate(totalSize, globalDataAlignment);

    // allocate gpu memory
    globalBufferAllocator.Allocate(totalSize, globalDataAlignment, descriptor.dataAlloc);

    const uint32_t* indices = submesh.GetIndices().data();
    const float3* positions = submesh.GetPositions().data();
    const unsigned char* attributes = submesh.GetAttribute().GetData().data();

    uint32_t sizeOffset = 0;

    memcpy(staging, indices, indexByteSize);
    descriptor.indexOffset = descriptor.dataAlloc.offset;
    sizeOffset += indexByteSize;

    uint32_t positionSize = submesh.GetPositions().size() * 3 * sizeof(float);
    memcpy(staging + sizeOffset, positions, positionSize);
    descriptor.positionOffset = descriptor.dataAlloc.offset + sizeOffset;
    sizeOffset += positionSize;

    uint32_t attributeSize = submesh.GetAttribute().GetSize();
    descriptor.attributeOffset = descriptor.dataAlloc.offset + sizeOffset;
    memcpy(staging + sizeOffset, attributes, attributeSize);

    GetGfxDriver()
        ->UploadBuffer(*globalBuffer, (uint8_t*)staging, totalSize, descriptor.dataAlloc.offset);
}

GPUDrivenManager& GPUDrivenManager::Instance()
{
    return *GetInstanceInternal();
}

void GPUDrivenManager::Deinit()
{
    GetInstanceInternal() = nullptr;
}

std::unique_ptr<GPUDrivenManager>& GPUDrivenManager::GetInstanceInternal()
{
    static std::unique_ptr<GPUDrivenManager> instance = std::unique_ptr<GPUDrivenManager>(new GPUDrivenManager());
    return instance;
}
} // namespace Rendering
