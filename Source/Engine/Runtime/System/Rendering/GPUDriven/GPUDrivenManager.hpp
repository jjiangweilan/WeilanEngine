#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Sampler.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Library/Allocators/VirtualTLSFAllocator.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include <cstddef>
#include <mutex>
#include <span>
#include <vector>

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
    uint32_t extraMaterialData = InvalidTextureIndex;
    uint32_t shaderHash;
    uint32_t _pad0;
    uint32_t _pad1;
};

static_assert(offsetof(GpuMaterial, extraMaterialData) == 76);
static_assert(offsetof(GpuMaterial, shaderHash) == 80);
static_assert(sizeof(GpuMaterial) == 92);

struct GpuGeometry
{
    uint32_t indexCount;
    uint32_t indexOffset;
    uint32_t positionOffset;
    uint32_t attributeOffset;

    uint32_t attributeStride;
    uint32_t attributeFlags;
    uint32_t normalOffset;
    uint32_t tangentOffset;
    uint32_t uvOffset;
    uint32_t boneOffset;

    static uint32_t GetNormalBit() { return 0x1; }
    static uint32_t GetTangentBit() { return 0x2; }
    static uint32_t GetHasUVBit() { return 0x4; }
    static uint32_t GetBoneBit() { return 0x8; }
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
    uint32_t skeletonOffset;
    uint32_t padding1;
};

struct GpuMaterialDescriptor
{
    VirtualTLSFAllocator::Allocation dataAlloc;
    VirtualTLSFAllocator::Allocation extraDataAlloc;

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

struct DrawIndexedIndirectCommand
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};

struct IndirectDrawData
{
    Gfx::Buffer* commandBuffer = nullptr;
    uint32_t firstDrawIndex = 0;
    uint32_t drawCount = 0;
};

struct GpuDrawExtra
{
    uint32_t objectOffset;
    uint32_t renderDataIndex;
};

struct GpuDynamicDataAllocation
{
    uint32_t offset = InvalidTextureIndex;
    uint32_t size = 0;

    bool IsValid() const { return offset != InvalidTextureIndex && size > 0; }
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
    static GPUDrivenManager* TryGetInstance();

    // Mesh registration (existing)
    GpuGeometryHandle RegisterGeometry(const Submesh& submesh);
    void UnregisterGeometry(GpuGeometryHandle handle);

    // Texture registration (bindless globalTextures[])
    GPUTextureHandle RegisterTexture(Texture& texture);
    void UpdateTextureImage(GPUTextureHandle handle, Texture& texture);
    void UnregisterTexture(GPUTextureHandle handle);

    // Material registration (data stored in globalBuffer)
    GPUMaterialHandle RegisterMaterial(
        const GpuMaterial& data,
        std::span<const uint8_t> extraData = {}
    );
    void UpdateMaterial(
        GPUMaterialHandle handle,
        const GpuMaterial& data,
        std::span<const uint8_t> extraData = {}
    );
    void UnregisterMaterial(GPUMaterialHandle handle);

    // Scene object registration (data stored in globalBuffer)
    GpuObjectHandle RegisterObject(
        const float4x4& modell,
        const float4x4& invTspModel,
        GpuRenderDataListHandle renderDataListHandle,
        uint32_t skeletonOffset = InvalidTextureIndex
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
    Gfx::Buffer* GetGlobalDynamicBuffer() { return globalDynamicBuffer.get(); }
    Gfx::ShaderResource* GetGlobalDescriptorSet() { return globalDescriptorSet.get(); }
    Gfx::Buffer* GetGPUDrivenConfigBuffer() { return gpuDrivenConfigBuffer.get(); }

    Gfx::Buffer* GetSceneBuffer() { return sceneBuffer.get(); }
    Gfx::Buffer* GetCameraBuffer() { return cameraBuffer.get(); }
    Gfx::Buffer* GetMainLightShadowBuffer() { return mainLightShadowBuffer.get(); }
    Gfx::Buffer* GetIndirectCommandBuffer() { return indirectCommandBuffer.get(); }
    Gfx::Buffer* GetIndirectCommandExtraBuffer() { return indirectCommandExtraBuffer.get(); }

    void SetObjectOffsetBuffer(Gfx::Buffer* buffer);
    void SetRTObjectOffsetBuffer(Gfx::Buffer* buffer);
    GpuDynamicDataAllocation UploadDynamicData(
        Gfx::CommandBuffer& cmd,
        const void* data,
        uint32_t size,
        uint32_t alignment = 16
    );
    IndirectDrawData UploadIndirectDrawData(
        Gfx::CommandBuffer& cmd,
        std::span<const DrawIndexedIndirectCommand> commands,
        std::span<const GpuDrawExtra> drawExtras
    );
    void Deinit();

    // Texture slot (bindless array element)
    struct TextureSlot
    {
        Texture* texture = nullptr;
    };

    // Debug / introspection accessors
    uint32_t GetGlobalBufferSize() const { return globalBufferSize; }
    uint32_t GetGlobalDynamicBufferSize() const { return globalDynamicBufferSize; }
    uint32_t GetGlobalDynamicBufferOffset() const { return globalDynamicBufferOffset; }
    uint64_t GetIndirectArenaFrameIndex() const { return indirectArenaFrameIndex; }
    uint32_t GetIndirectCommandBufferCapacity() const { return indirectCommandBufferCapacity; }
    uint32_t GetIndirectCommandBufferOffset() const { return indirectCommandBufferOffset; }
    bool IsGPUDrivenConfigDirty() const { return gpuDrivenConfigDirty; }

    const ObjectPool<GpuGeometryDescriptor>& GetGeometryPool() const { return geometryDescriptors; }
    const ObjectPool<GpuMaterialDescriptor>& GetMaterialPool() const { return materialDescriptortors; }
    const ObjectPool<GpuObjectDescriptor>& GetObjectPool() const { return objectDescriptors; }
    const ObjectPool<GpuRenderDataListDescriptor>& GetRenderDataListPool() const { return renderDataListDescriptors; }
    const ObjectPool<TextureSlot>& GetTexturePool() const { return textureSlots; }

private:
    GPUDrivenManager();
    std::mutex mutex;

    // Global buffer (512MB TLSF)
    const uint8_t globalDataAlignment = 16;
    uint32_t globalBufferSize = 512 * 1024 * 1024;
    VirtualTLSFAllocator globalBufferAllocator{globalBufferSize};
    std::unique_ptr<Gfx::Buffer> globalBuffer;
    uint64_t globalBufferShaderDeviceAddress = 0;

    uint32_t globalDynamicBufferSize = 64 * 1024 * 1024;
    std::unique_ptr<Gfx::Buffer> globalDynamicBuffer;
    uint64_t globalDynamicBufferFrameIndex = 0;
    uint32_t globalDynamicBufferOffset = 0;

    // Global descriptor set (set 0)
    std::unique_ptr<Gfx::ShaderResource> globalDescriptorSet;

    // Global sampler table (matches PerScene.hlsl globalSamplers[11])
    // Layout: index = addressMode * 2 + filterMode
    //   addressMode: Repeat=0, MirroredRepeat=1, ClampToEdge=2, ClampToBorder=3
    //   filterMode:  Nearest=0, Linear=1
    //   Indices 8/9 are fallback for MirrorClampToEdge (maps to ClampToEdge)
    //   Index 10 is anisotropic repeat.
    static constexpr int GlobalSamplerCount = 11;
    std::unique_ptr<Gfx::Sampler> globalSamplers[GlobalSamplerCount];

    // GPUDriven config buffer
    std::unique_ptr<Gfx::Buffer> gpuDrivenConfigBuffer;
    bool gpuDrivenConfigDirty = true;

    std::unique_ptr<Gfx::Buffer> sceneBuffer;
    std::unique_ptr<Gfx::Buffer> cameraBuffer;
    std::unique_ptr<Gfx::Buffer> mainLightShadowBuffer;
    std::unique_ptr<Gfx::Buffer> indirectCommandBuffer;
    std::unique_ptr<Gfx::Buffer> indirectCommandExtraBuffer;
    uint64_t indirectArenaFrameIndex = 0;
    uint32_t indirectCommandBufferCapacity = 0;
    uint32_t indirectCommandBufferOffset = 0;

    void BeginGlobalDynamicBufferFrame();
    bool EnsureGlobalDynamicBufferCapacity(uint32_t requiredSize);
    void BeginIndirectArenaFrame();
    bool EnsureIndirectCommandCapacity(uint32_t requiredSize);

    // Mesh data
    ObjectPool<GpuGeometryDescriptor> geometryDescriptors;
    void AllocateForMesh(GpuGeometryDescriptor& descriptor, const Submesh& submesh);

    ObjectPool<GpuMaterialDescriptor> materialDescriptortors;
    void UploadMaterial(GPUMaterialHandle handle);
    void UpdateMaterialExtraData(
        GpuMaterialDescriptor& descriptor,
        std::span<const uint8_t> extraData
    );

    ObjectPool<GpuObjectDescriptor> objectDescriptors;
    void UploadObject(GpuObjectHandle handle);

    ObjectPool<GpuRenderDataListDescriptor> renderDataListDescriptors;
    void UploadRenderData(GpuRenderDataListHandle handle);

    // Texture data (bindless array)
    ObjectPool<TextureSlot> textureSlots;

    static std::unique_ptr<GPUDrivenManager>& GetInstanceInternal();
};
} // namespace Rendering
