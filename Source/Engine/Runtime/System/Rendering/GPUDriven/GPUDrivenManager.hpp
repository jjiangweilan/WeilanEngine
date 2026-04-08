#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/Sampler.hpp"
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
    glm::uvec2 baseColorTexIndex;
    glm::uvec2 normalMapTexIndex;
    glm::uvec2 metallicRoughnessTexIndex;
    glm::uvec2 emissiveMapTexIndex;
    uint32_t shaderHash;
    uint32_t _pad0;
};

struct GpuGeometry
{
    uint32_t indexCount;
    uint32_t indexOffset;
    uint32_t positionOffset;
    uint32_t attributeOffset;

    uint32_t attributeStride;
    uint32_t attributeFlags;
    uint32_t padding0;
    uint32_t padding1;

    static uint32_t GetNormalBit() { return 0x1; }
    static uint32_t GetTangentBit() { return 0x2; }
    static uint32_t GetHasUVBit() { return 0x4; }
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

    Gfx::Buffer* GetSceneBuffer() { return sceneBuffer.get(); }
    Gfx::Buffer* GetCameraBuffer() { return cameraBuffer.get(); }
    Gfx::Buffer* GetMainLightShadowBuffer() { return mainLightShadowBuffer.get(); }

    void SetObjectOffsetBuffer(Gfx::Buffer* buffer);
    void SetRTObjectOffsetBuffer(Gfx::Buffer* buffer);

    void Deinit();

private:
    GPUDrivenManager();
    std::mutex mutex;

    // Global buffer (512MB TLSF)
    const uint8_t globalDataAlignment = 4;
    uint32_t globalBufferSize = 512 * 1024 * 1024;
    VirtualTLSFAllocator globalBufferAllocator{globalBufferSize};
    std::unique_ptr<Gfx::Buffer> globalBuffer;
    uint64_t globalBufferShaderDeviceAddress = 0;

    // Global descriptor set (set 0)
    std::unique_ptr<Gfx::ShaderResource> globalDescriptorSet;

    // Global sampler table (matches PerScene.hlsl globalSamplers[10])
    // Layout: index = addressMode * 2 + filterMode
    //   addressMode: Repeat=0, MirroredRepeat=1, ClampToEdge=2, ClampToBorder=3
    //   filterMode:  Nearest=0, Linear=1
    //   Indices 8/9 are fallback for MirrorClampToEdge (maps to ClampToEdge)
    static constexpr int GlobalSamplerCount = 10;
    std::unique_ptr<Gfx::Sampler> globalSamplers[GlobalSamplerCount];

    // GPUDriven config buffer
    std::unique_ptr<Gfx::Buffer> gpuDrivenConfigBuffer;
    bool gpuDrivenConfigDirty = true;

    std::unique_ptr<Gfx::Buffer> sceneBuffer;
    std::unique_ptr<Gfx::Buffer> cameraBuffer;
    std::unique_ptr<Gfx::Buffer> mainLightShadowBuffer;

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
