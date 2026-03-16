#include "GPUDrivenManager.hpp"
#include "Engine/Library/Allocators/ThreadLocalAllocator.hpp"

namespace Rendering
{
GPUMeshHandle GPUDrivenManager::RegisterMesh(Submesh& submesh)
{
    GPUMeshHandle handle = sceneObjectVertexDataDescriptors.AllocateRaw();
    SceneObjectVertexDataDescriptor& newDescriptor = sceneObjectVertexDataDescriptors[handle];
    AllocateForMesh(newDescriptor, submesh);

    return handle;
}

void GPUDrivenManager::UnregisterMesh(GPUMeshHandle handle)
{
    SceneObjectVertexDataDescriptor& descriptor = sceneObjectVertexDataDescriptors[handle];
    globalBufferAllocator.Free(descriptor.dataAlloc);
    sceneObjectVertexDataDescriptors.FreeRaw(static_cast<int>(handle));
}

void GPUDrivenManager::AllocateForMesh(SceneObjectVertexDataDescriptor& descriptor, Submesh& submesh)
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
    sizeOffset += indexByteSize;

    uint32_t positionSize = submesh.GetPositions().size() * 3 * sizeof(float);
    memcpy(staging + sizeOffset, positions, positionSize);
    sizeOffset += positionSize;

    uint32_t attributeSize = submesh.GetAttribute().GetSize();
    memcpy(staging + sizeOffset, attributes, attributeSize);

    GetGfxDriver()
        ->UploadBuffer(*globalBuffer, (uint8_t*)staging, totalSize, descriptor.dataAlloc.offset);
}
} // namespace Rendering
