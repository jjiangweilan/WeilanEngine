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

using GpuGeometryHandle = ObjectPoolRawHandle;
using GPUTextureHandle = ObjectPoolRawHandle;
using GPUMaterialHandle = ObjectPoolRawHandle;
using GpuObjectHandle = ObjectPoolRawHandle;
using GpuRenderDataListHandle = ObjectPoolRawHandle;

constexpr GpuGeometryHandle InvalidGPUHandle = static_cast<GpuGeometryHandle>(-1);
constexpr uint32_t InvalidTextureIndex = 0xFFFFFFFF;

struct GpuMaterial
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
    uint32_t shaderHash;
};

struct GpuGeometry
{
    uint32_t indexCount;
    uint32_t indexOffset;
    uint32_t positionOffset;
    uint32_t attributeOffset;
};

struct GpuRenderData
{
    uint32_t geometryOffset;
    uint32_t materialOffset;
    uint32_t shaderID;
    uint32_t padding0;
};

struct GpuObject
{
    float4x4 model;
    float4x4 invTspModel;
    uint32_t renderDataCount;
    uint32_t pRenderDataOffset;
    uint32_t padding0;
    uint32_t padding1;
};

struct GpuMaterialDescriptor
{
    VirtualTLSFAllocator::Allocation dataAlloc;

    GpuMaterial materialData;
};

struct GpuRenderDataListDescriptor
{
    VirtualTLSFAllocator::Allocation dataAlloc;

    struct GpuRenderDataHandles
    {
        Rendering::GpuGeometryHandle geometryHandle;
        Rendering::GpuGeometryHandle materialHandle;
    };

    std::vector<GpuRenderDataHandles> renderDataHandles;
    std::vector<GpuRenderData> renderDataList;
};

struct GpuObjectDescriptor
{
    VirtualTLSFAllocator::Allocation dataAlloc;

    GpuRenderDataListHandle renderDataListHandle;
    GpuObject gpuObject;
};

struct GpuGeometryDescriptor
{
    VirtualTLSFAllocator::Allocation dataAlloc;

    GpuGeometry geometry;
};

class GPUDrivenManager
{
public:
    static GPUDrivenManager& Instance();

    // Mesh registration (existing)
    GpuGeometryHandle RegisterGeometry(const Submesh& submesh);
    void UnregisterGeometry(GpuGeometryHandle handle);

    // Texture registration (bindless globalTextures[])
    GPUTextureHandle RegisterTexture(Texture& texture);
    void UpdateTextureImage(GPUTextureHandle handle, Texture& texture);
    void UnregisterTexture(GPUTextureHandle handle);

    // Material registration (data stored in globalBuffer)
    GPUMaterialHandle RegisterMaterial(const GpuMaterial& data);
    void UpdateMaterial(GPUMaterialHandle handle, const GpuMaterial& data);
    void UnregisterMaterial(GPUMaterialHandle handle);

    // Scene object registration (data stored in globalBuffer)
    GpuObjectHandle RegisterObject(
        const float4x4& modell,
        const float4x4& invTspModel,
        GpuRenderDataListHandle renderDataListHandle
    );
    void UpdateObject(GpuObjectHandle handle, const GpuObject& data);
    void UnregisterObject(GpuObjectHandle handle);

    GpuRenderDataListHandle RegisterRenderDataList(const std::vector<GpuRenderData>& data);
    void UnregisterRenderDataList(GpuRenderDataListHandle handle);

    uint64_t GetGlobalBufferShaderDeviceAddress();

    GpuGeometryDescriptor GetGeometryDescriptor(GpuGeometryHandle handle)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return geometryDescriptors[handle];
    }

    GpuObjectDescriptor GetObjectDescriptor(GpuObjectHandle handle)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return objectDescriptors[handle];
    }

    GpuMaterialDescriptor GetMaterialDescriptor(GPUMaterialHandle handle)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return materialDescriptortors[handle];
    }

    GpuRenderDataListDescriptor GetRenderDataListDescriptor(GpuRenderDataListHandle handle)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return renderDataListDescriptors[handle];
    }

    Gfx::Buffer* GetGlobalBuffer() { return globalBuffer.get(); }
    Gfx::ShaderResource* GetGlobalDescriptorSet() { return globalDescriptorSet.get(); }
    Gfx::Buffer* GetGPUDrivenConfigBuffer() { return gpuDrivenConfigBuffer.get(); }

    // Bind camera/scene/shadow buffers into the global descriptor set.
    // Ownership of these buffers remains with the caller (PerScene).
    void SetSceneBuffer(Gfx::Buffer* buffer);
    void SetCameraBuffer(Gfx::Buffer* buffer);
    void SetMainLightShadowBuffer(Gfx::Buffer* buffer);
    void SetObjectOffsetBuffer(Gfx::Buffer* buffer);

    void Deinit();

private:
    GPUDrivenManager();
    std::mutex mutex;

    // Global buffer (512MB TLSF)
    const uint8_t globalDataAlignment = 16;
    uint32_t globalBufferSize = 512 * 1024 * 1024;
    VirtualTLSFAllocator globalBufferAllocator{globalBufferSize};
    std::unique_ptr<Gfx::Buffer> globalBuffer;
    uint64_t globalBufferShaderDeviceAddress = 0;

    // Global descriptor set (set 0)
    std::unique_ptr<Gfx::ShaderResource> globalDescriptorSet;

    // GPUDriven config buffer
    std::unique_ptr<Gfx::Buffer> gpuDrivenConfigBuffer;
    bool gpuDrivenConfigDirty = true;

    // Mesh data
    ObjectPool<GpuGeometryDescriptor> geometryDescriptors;
    void AllocateForMesh(GpuGeometryDescriptor& descriptor, const Submesh& submesh);

    ObjectPool<GpuMaterialDescriptor> materialDescriptortors;
    void UploadMaterial(GPUMaterialHandle handle);

    ObjectPool<GpuObjectDescriptor> objectDescriptors;
    void UploadObject(GpuObjectHandle handle);

    ObjectPool<GpuRenderDataListDescriptor> renderDataListDescriptors;
    void UploadRenderData(GpuRenderDataListHandle handle);

    // Texture data (bindless array)
    struct TextureSlot
    {
        Texture* texture = nullptr;
    };
    ObjectPool<TextureSlot> textureSlots;

    static std::unique_ptr<GPUDrivenManager>& GetInstanceInternal();
};
} // namespace Rendering
