#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Library/Allocators/VirtualTLSFAllocator.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include <mutex>

class Submesh;
namespace Rendering
{

using GPUMeshHandle = ObjectPoolRawHandle;
using GPUTextureHandle = ObjectPoolRawHandle;
using GPUMaterialHandle = ObjectPoolRawHandle;
using GPUSceneObjectHandle = ObjectPoolRawHandle;

constexpr GPUMeshHandle InvalidGPUHandle = static_cast<GPUMeshHandle>(-1);
constexpr uint32_t InvalidTextureIndex = 0xFFFFFFFF;

// C++ mirror of GPUMaterialData in GPUDrivenStructures.hlsl
struct GPUMaterialData
{
    glm::vec4 baseColorFactor;
    glm::vec4 emissive;
    float roughness;
    float metallic;
    float alphaCutoff;
    uint32_t baseColorTexIndex;
    uint32_t normalMapTexIndex;
    uint32_t metallicRoughnessTexIndex;
    uint32_t emissiveMapTexIndex;
    uint32_t padding;
};

// C++ mirror of GPUSceneObjectData in GPUDrivenStructures.hlsl
struct GPUSceneObjectData
{
    glm::mat4 model;
    glm::mat4 invTspModel; // [3].xyz encodes mesh offsets (index, position, attribute)
    uint32_t materialIndex;
    uint32_t padding[3];
};

// C++ mirror of GPUDrivenConfig in GPUDrivenStructures.hlsl
struct GPUDrivenConfig
{
    uint32_t materialDataBaseOffset;
    uint32_t sceneObjectDataBaseOffset;
    uint32_t sceneObjectCount;
    uint32_t materialCount;
};

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

    // Mesh registration (existing)
    GPUMeshHandle RegisterMesh(const Submesh& submesh);
    void UnregisterMesh(GPUMeshHandle handle);

    // Texture registration (bindless globalTextures[])
    GPUTextureHandle RegisterTexture(Texture& texture);
    void UnregisterTexture(GPUTextureHandle handle);
    void UpdateTextureImage(GPUTextureHandle handle, Texture& texture);

    // Material registration (data stored in globalBuffer)
    GPUMaterialHandle RegisterMaterial(const GPUMaterialData& data);
    void UpdateMaterial(GPUMaterialHandle handle, const GPUMaterialData& data);
    void UnregisterMaterial(GPUMaterialHandle handle);

    // Scene object registration (data stored in globalBuffer)
    GPUSceneObjectHandle RegisterSceneObject(const GPUSceneObjectData& data);
    void UpdateSceneObject(GPUSceneObjectHandle handle, const GPUSceneObjectData& data);
    void UnregisterSceneObject(GPUSceneObjectHandle handle);

    const SceneObjectVertexDataDescriptor& GetSceneObjectVertexDataDescriptor(GPUMeshHandle handle)
    {
        return sceneObjectVertexDataDescriptors[handle];
    }

    Gfx::Buffer* GetGlobalBuffer() { return globalBuffer.get(); }
    Gfx::ShaderResource* GetGlobalDescriptorSet() { return globalDescriptorSet.get(); }
    Gfx::Buffer* GetGPUDrivenConfigBuffer() { return gpuDrivenConfigBuffer.get(); }
    Gfx::Buffer* GetObjectIDBuffer() { return objectIDBuffer.get(); }

    // Update the object ID buffer for the current frame. Called by RenderPipeline.
    void UploadObjectIDs(const uint32_t* ids, uint32_t count);

    // Bind camera/scene/shadow buffers into the global descriptor set.
    // Ownership of these buffers remains with the caller (PerScene).
    void SetSceneBuffer(Gfx::Buffer* buffer);
    void SetCameraBuffer(Gfx::Buffer* buffer);
    void SetMainLightShadowBuffer(Gfx::Buffer* buffer);

    void Deinit();
    void UploadGPUDrivenConfig();

private:
    GPUDrivenManager();
    std::mutex mutex;

    // Global buffer (512MB TLSF)
    const uint8_t globalDataAlignment = 8;
    uint32_t globalBufferSize = 512 * 1024 * 1024;
    VirtualTLSFAllocator globalBufferAllocator{globalBufferSize};
    std::unique_ptr<Gfx::Buffer> globalBuffer;

    // Global descriptor set (set 0)
    std::unique_ptr<Gfx::ShaderResource> globalDescriptorSet;

    // GPUDriven config buffer
    GPUDrivenConfig gpuDrivenConfigData{};
    std::unique_ptr<Gfx::Buffer> gpuDrivenConfigBuffer;
    bool gpuDrivenConfigDirty = true;

    // Object ID buffer (per-frame, CPU-visible)
    std::unique_ptr<Gfx::Buffer> objectIDBuffer;
    uint32_t objectIDBufferCapacity = 0;
    static constexpr uint32_t InitialObjectIDCapacity = 1024;

    // Mesh data
    ObjectPool<SceneObjectVertexDataDescriptor> sceneObjectVertexDataDescriptors;
    void AllocateForMesh(SceneObjectVertexDataDescriptor& descriptor, const Submesh& submesh);

    // Texture data (bindless array)
    struct TextureSlot
    {
        Texture* texture = nullptr;
    };
    ObjectPool<TextureSlot> textureSlots;

    // Material data (contiguous region in globalBuffer)
    struct MaterialSlot
    {
        GPUMaterialData data{};
    };
    ObjectPool<MaterialSlot> materialSlots;
    VirtualTLSFAllocator::Allocation materialBlockAlloc{};
    uint32_t materialBlockCapacity = 0;
    static constexpr uint32_t InitialMaterialCapacity = 256;
    void EnsureMaterialCapacity(uint32_t requiredCount);
    void UploadMaterialData(GPUMaterialHandle handle);

    // Scene object data (contiguous region in globalBuffer)
    struct SceneObjectSlot
    {
        GPUSceneObjectData data{};
    };
    ObjectPool<SceneObjectSlot> sceneObjectSlots;
    VirtualTLSFAllocator::Allocation sceneObjectBlockAlloc{};
    uint32_t sceneObjectBlockCapacity = 0;
    static constexpr uint32_t InitialSceneObjectCapacity = 1024;
    void EnsureSceneObjectCapacity(uint32_t requiredCount);
    void UploadSceneObjectData(GPUSceneObjectHandle handle);

    static std::unique_ptr<GPUDrivenManager>& GetInstanceInternal();
};
} // namespace Rendering
