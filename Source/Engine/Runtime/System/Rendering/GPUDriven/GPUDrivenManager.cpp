#include "GPUDrivenManager.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/ResourceHandle.hpp"
#include "Engine/Library/Allocators/ThreadLocalAllocator.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"

namespace Rendering
{

GPUDrivenManager::GPUDrivenManager()
{
    globalBuffer = GetGfxDriver()->CreateBuffer(
        globalBufferSize,
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        false,
        true,
        "GPUDrivenGlobalBuffer"
    );

    // Create global descriptor set (set 0)
    globalDescriptorSet = GetGfxDriver()->CreateShaderResource();
    globalDescriptorSet->SetBuffer("globalBuffer", globalBuffer.get());

    // GPUDriven config uniform buffer
    gpuDrivenConfigBuffer = GetGfxDriver()->CreateBuffer(
        sizeof(GPUDrivenConfig),
        Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
        false,
        false,
        "GPUDrivenConfig"
    );
    globalDescriptorSet->SetBuffer("gpuDrivenConfig", gpuDrivenConfigBuffer.get());

    // Object ID buffer (initial capacity)
    objectIDBufferCapacity = InitialObjectIDCapacity;
    objectIDBuffer = GetGfxDriver()->CreateBuffer(
        objectIDBufferCapacity * sizeof(uint32_t),
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        true,
        false,
        "ObjectIDBuffer"
    );
    globalDescriptorSet->SetBuffer("objectIDs", objectIDBuffer.get());

    // Pre-allocate material block in globalBuffer
    EnsureMaterialCapacity(InitialMaterialCapacity);

    // Pre-allocate scene object block in globalBuffer
    EnsureSceneObjectCapacity(InitialSceneObjectCapacity);

    UploadGPUDrivenConfig();
}

// --- Mesh registration (existing) ---

GPUMeshHandle GPUDrivenManager::RegisterMesh(const Submesh& submesh)
{
    std::lock_guard<std::mutex> lock(mutex);
    GPUMeshHandle handle = sceneObjectVertexDataDescriptors.AllocateRaw();
    SceneObjectVertexDataDescriptor& newDescriptor = sceneObjectVertexDataDescriptors[handle];
    AllocateForMesh(newDescriptor, submesh);
    return handle;
}

void GPUDrivenManager::UnregisterMesh(GPUMeshHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
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

    uint8_t* staging = (uint8_t*)tempAllocator.allocate(totalSize, globalDataAlignment);
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

// --- Texture registration (bindless) ---

GPUTextureHandle GPUDrivenManager::RegisterTexture(Texture& texture)
{
    std::lock_guard<std::mutex> lock(mutex);
    GPUTextureHandle handle = textureSlots.AllocateRaw();
    textureSlots[handle].texture = &texture;

    globalDescriptorSet->SetImage("globalTextures"_shaderBinding, static_cast<int>(handle), &texture.GetGfxImage()->GetDefaultImageView());

    return handle;
}

void GPUDrivenManager::UnregisterTexture(GPUTextureHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    textureSlots[handle].texture = nullptr;
    textureSlots.FreeRaw(static_cast<int>(handle));
}

void GPUDrivenManager::UpdateTextureImage(GPUTextureHandle handle, Texture& texture)
{
    std::lock_guard<std::mutex> lock(mutex);
    textureSlots[handle].texture = &texture;
    globalDescriptorSet->SetImage("globalTextures"_shaderBinding, static_cast<int>(handle), &texture.GetGfxImage()->GetDefaultImageView());
}

// --- Material registration ---

void GPUDrivenManager::EnsureMaterialCapacity(uint32_t requiredCount)
{
    if (requiredCount <= materialBlockCapacity)
        return;

    uint32_t newCapacity = materialBlockCapacity == 0 ? InitialMaterialCapacity : materialBlockCapacity;
    while (newCapacity < requiredCount)
        newCapacity *= 2;

    VirtualTLSFAllocator::Allocation newAlloc{};
    globalBufferAllocator.Allocate(
        newCapacity * sizeof(GPUMaterialData), globalDataAlignment, newAlloc
    );

    // Free old allocation if any
    if (materialBlockAlloc.IsValid())
    {
        globalBufferAllocator.Free(materialBlockAlloc);
    }

    materialBlockAlloc = newAlloc;
    materialBlockCapacity = newCapacity;

    gpuDrivenConfigData.materialDataBaseOffset = static_cast<uint32_t>(materialBlockAlloc.offset);
    gpuDrivenConfigDirty = true;

    // Re-upload all existing materials to new location using actual pool indices
    for (auto it = materialSlots.begin(); it != materialSlots.end(); ++it)
    {
        uint32_t poolIndex = static_cast<uint32_t>(it.index);
        uint32_t offset = static_cast<uint32_t>(materialBlockAlloc.offset) + poolIndex * sizeof(GPUMaterialData);
        GetGfxDriver()->UploadBuffer(
            *globalBuffer, reinterpret_cast<uint8_t*>(&it->data), sizeof(GPUMaterialData), offset
        );
    }
}

void GPUDrivenManager::UploadMaterialData(GPUMaterialHandle handle)
{
    uint32_t offset =
        static_cast<uint32_t>(materialBlockAlloc.offset) + static_cast<uint32_t>(handle) * sizeof(GPUMaterialData);
    GetGfxDriver()->UploadBuffer(
        *globalBuffer,
        reinterpret_cast<uint8_t*>(&materialSlots[handle].data),
        sizeof(GPUMaterialData),
        offset
    );
}

GPUMaterialHandle GPUDrivenManager::RegisterMaterial(const GPUMaterialData& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    GPUMaterialHandle handle = materialSlots.AllocateRaw();
    materialSlots[handle].data = data;

    EnsureMaterialCapacity(static_cast<uint32_t>(handle) + 1);
    UploadMaterialData(handle);

    gpuDrivenConfigData.materialCount = static_cast<uint32_t>(materialSlots.GetUsedCount());
    gpuDrivenConfigDirty = true;

    return handle;
}

void GPUDrivenManager::UpdateMaterial(GPUMaterialHandle handle, const GPUMaterialData& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    materialSlots[handle].data = data;
    UploadMaterialData(handle);
}

void GPUDrivenManager::UnregisterMaterial(GPUMaterialHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    materialSlots.FreeRaw(static_cast<int>(handle));

    gpuDrivenConfigData.materialCount = static_cast<uint32_t>(materialSlots.GetUsedCount());
    gpuDrivenConfigDirty = true;
}

// --- Scene object registration ---

void GPUDrivenManager::EnsureSceneObjectCapacity(uint32_t requiredCount)
{
    if (requiredCount <= sceneObjectBlockCapacity)
        return;

    uint32_t newCapacity = sceneObjectBlockCapacity == 0 ? InitialSceneObjectCapacity : sceneObjectBlockCapacity;
    while (newCapacity < requiredCount)
        newCapacity *= 2;

    VirtualTLSFAllocator::Allocation newAlloc{};
    globalBufferAllocator.Allocate(
        newCapacity * sizeof(GPUSceneObjectData), globalDataAlignment, newAlloc
    );

    if (sceneObjectBlockAlloc.IsValid())
    {
        globalBufferAllocator.Free(sceneObjectBlockAlloc);
    }

    sceneObjectBlockAlloc = newAlloc;
    sceneObjectBlockCapacity = newCapacity;

    gpuDrivenConfigData.sceneObjectDataBaseOffset = static_cast<uint32_t>(sceneObjectBlockAlloc.offset);
    gpuDrivenConfigDirty = true;

    // Re-upload all existing scene objects to new location using actual pool indices
    for (auto it = sceneObjectSlots.begin(); it != sceneObjectSlots.end(); ++it)
    {
        uint32_t poolIndex = static_cast<uint32_t>(it.index);
        uint32_t offset =
            static_cast<uint32_t>(sceneObjectBlockAlloc.offset) + poolIndex * sizeof(GPUSceneObjectData);
        GetGfxDriver()->UploadBuffer(
            *globalBuffer, reinterpret_cast<uint8_t*>(&it->data), sizeof(GPUSceneObjectData), offset
        );
    }
}

void GPUDrivenManager::UploadSceneObjectData(GPUSceneObjectHandle handle)
{
    uint32_t offset = static_cast<uint32_t>(sceneObjectBlockAlloc.offset) +
                      static_cast<uint32_t>(handle) * sizeof(GPUSceneObjectData);
    GetGfxDriver()->UploadBuffer(
        *globalBuffer,
        reinterpret_cast<uint8_t*>(&sceneObjectSlots[handle].data),
        sizeof(GPUSceneObjectData),
        offset
    );
}

GPUSceneObjectHandle GPUDrivenManager::RegisterSceneObject(const GPUSceneObjectData& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    GPUSceneObjectHandle handle = sceneObjectSlots.AllocateRaw();
    sceneObjectSlots[handle].data = data;

    EnsureSceneObjectCapacity(static_cast<uint32_t>(handle) + 1);
    UploadSceneObjectData(handle);

    gpuDrivenConfigData.sceneObjectCount = static_cast<uint32_t>(sceneObjectSlots.GetUsedCount());
    gpuDrivenConfigDirty = true;

    return handle;
}

void GPUDrivenManager::UpdateSceneObject(GPUSceneObjectHandle handle, const GPUSceneObjectData& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    sceneObjectSlots[handle].data = data;
    UploadSceneObjectData(handle);
}

void GPUDrivenManager::UnregisterSceneObject(GPUSceneObjectHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    sceneObjectSlots.FreeRaw(static_cast<int>(handle));

    gpuDrivenConfigData.sceneObjectCount = static_cast<uint32_t>(sceneObjectSlots.GetUsedCount());
    gpuDrivenConfigDirty = true;
}

// --- Object ID buffer ---

void GPUDrivenManager::UploadObjectIDs(const uint32_t* ids, uint32_t count)
{
    if (count == 0)
        return;

    // Grow buffer if needed
    if (count > objectIDBufferCapacity)
    {
        uint32_t newCapacity = objectIDBufferCapacity;
        while (newCapacity < count)
            newCapacity *= 2;

        objectIDBuffer = GetGfxDriver()->CreateBuffer(
            newCapacity * sizeof(uint32_t),
            Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
            true,
            false,
            "ObjectIDBuffer"
        );
        objectIDBufferCapacity = newCapacity;
        globalDescriptorSet->SetBuffer("objectIDs", objectIDBuffer.get());
    }

    // Write directly to mapped memory
    void* mapped = objectIDBuffer->GetCPUVisibleAddress();
    if (mapped)
    {
        memcpy(mapped, ids, count * sizeof(uint32_t));
    }
}

// --- Descriptor set passthrough for scene buffers ---

void GPUDrivenManager::SetSceneBuffer(Gfx::Buffer* buffer)
{
    globalDescriptorSet->SetBuffer("scene", buffer);
}

void GPUDrivenManager::SetCameraBuffer(Gfx::Buffer* buffer)
{
    globalDescriptorSet->SetBuffer("camera", buffer);
}

void GPUDrivenManager::SetMainLightShadowBuffer(Gfx::Buffer* buffer)
{
    globalDescriptorSet->SetBuffer("mainLightShadow", buffer);
}

// --- Config upload ---

void GPUDrivenManager::UploadGPUDrivenConfig()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!gpuDrivenConfigDirty)
        return;

    gpuDrivenConfigDirty = false;
    GetGfxDriver()->UploadBuffer(
        *gpuDrivenConfigBuffer,
        reinterpret_cast<uint8_t*>(&gpuDrivenConfigData),
        sizeof(GPUDrivenConfig),
        0
    );
}

// --- Lifecycle ---

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
