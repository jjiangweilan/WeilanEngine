#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/ResourceHandle.hpp"
#include "Engine/Library/Allocators/ThreadLocalAllocator.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/GPUParameter.hpp"
#include <spdlog/spdlog.h>

namespace Rendering
{

GPUDrivenManager::GPUDrivenManager()
{
    globalBuffer = GetGfxDriver()->CreateBuffer(
        globalBufferSize,
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::ShaderDeviceAddress | Gfx::BufferUsage::AccelerationStructureBuildInput | Gfx::BufferUsage::Index,
        false,
        true,
        "GPUDrivenGlobalBuffer"
    );
    globalDynamicBuffer = GetGfxDriver()->CreateBuffer(
        globalDynamicBufferSize,
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        false,
        false,
        "GPUDrivenGlobalDynamicBuffer"
    );

    // Create global descriptor set (set 0)
    globalDescriptorSet = GetGfxDriver()->CreateShaderResource();
    globalDescriptorSet->SetBuffer("globalBuffer", globalBuffer.get());
    globalDescriptorSet->SetBuffer("globalDynamicBuffer", globalDynamicBuffer.get());

    // Create global sampler table matching PerScene.hlsl globalSamplers[11].
    // Layout: index = addressMode * 2 + filterMode
    //   addressMode: Repeat=0, MirroredRepeat=1, ClampToEdge=2, ClampToBorder=3
    //   filterMode:  Nearest=0, Linear=1
    //   Indices 8 and 9 are fallbacks for MirrorClampToEdge (unsupported), mapped to ClampToEdge.
    //   Index 10 is linear anisotropic repeat.
    using AM = Gfx::SamplerAddressMode;
    using FM = Gfx::FilterMode;
    struct SamplerDesc
    {
        AM addr;
        FM filter;
        bool anisotropic = false;
    };
    const SamplerDesc descs[GlobalSamplerCount] = {
        {AM::Repeat, FM::Nearest},         // 0 point_repeat
        {AM::Repeat, FM::Linear},          // 1 linear_repeat
        {AM::MirroredRepeat, FM::Nearest}, // 2 point_mirror
        {AM::MirroredRepeat, FM::Linear},  // 3 linear_mirror
        {AM::ClampToEdge, FM::Nearest},    // 4 point_clamp
        {AM::ClampToEdge, FM::Linear},     // 5 linear_clamp
        {AM::ClampToBorder, FM::Nearest},  // 6 point_border
        {AM::ClampToBorder, FM::Linear},   // 7 linear_border
        {AM::ClampToEdge, FM::Nearest},    // 8 fallback for MirrorClampToEdge (Nearest)
        {AM::ClampToEdge, FM::Linear},     // 9 fallback for MirrorClampToEdge (Linear)
        {AM::Repeat, FM::Linear, true},     // 10 anisotropic_repeat
    };
    for (int i = 0; i < GlobalSamplerCount; ++i)
    {
        Gfx::Sampler::CreateInfo ci{};
        ci.addressModeU = descs[i].addr;
        ci.addressModeV = descs[i].addr;
        ci.addressModeW = descs[i].addr;
        ci.minFilter = descs[i].filter;
        ci.magFilter = descs[i].filter;
        ci.anisotropic = descs[i].anisotropic;
        globalSamplers[i] = GetGfxDriver()->CreateSampler(ci);
        globalDescriptorSet->SetSampler("globalSamplers", i, globalSamplers[i].get());
    }

    sceneBuffer = GetGfxDriver()->CreateBuffer(sizeof(GPUParameter::Scene), Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst, false, true, "Scene");
    cameraBuffer = GetGfxDriver()->CreateBuffer(sizeof(GPUParameter::Camera), Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst, false, true, "Camera");
    mainLightShadowBuffer = GetGfxDriver()->CreateBuffer(sizeof(GPUParameter::MainLightShadow), Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst, false, true, "MainLightShadow");

    globalDescriptorSet->SetBuffer("scene", sceneBuffer.get());
    globalDescriptorSet->SetBuffer("camera", cameraBuffer.get());
    globalDescriptorSet->SetBuffer("mainLightShadow", mainLightShadowBuffer.get());
}

GpuRenderDataListHandle GPUDrivenManager::RegisterRenderDataList(const std::vector<GpuRenderData>& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    GpuRenderDataListHandle handle = renderDataListDescriptors.AllocateRaw();

    if (data.empty())
        return handle;

    auto& descriptor = renderDataListDescriptors[handle];

    globalBufferAllocator.Allocate(sizeof(GpuRenderData) * data.size(), globalDataAlignment, descriptor.dataAlloc);
    descriptor.renderDataList = data;

    GetGfxDriver()->UploadBuffer(
        *globalBuffer,
        reinterpret_cast<uint8_t*>(descriptor.renderDataList.data()),
        sizeof(GpuRenderData) * data.size(),
        descriptor.dataAlloc.offset
    );

    return handle;
}

void GPUDrivenManager::UnregisterRenderDataList(GpuRenderDataListHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& descriptor = renderDataListDescriptors[handle];
    globalBufferAllocator.Free(descriptor.dataAlloc);
    renderDataListDescriptors.FreeRaw(static_cast<int>(handle));
}

GpuGeometryHandle GPUDrivenManager::RegisterGeometry(const Submesh& submesh)
{
    std::lock_guard<std::mutex> lock(mutex);
    GpuGeometryHandle handle = geometryDescriptors.AllocateRaw();
    GpuGeometryDescriptor& newDescriptor = geometryDescriptors[handle];
    AllocateForMesh(newDescriptor, submesh);
    return handle;
}

void GPUDrivenManager::UnregisterGeometry(GpuGeometryHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    GpuGeometryDescriptor& descriptor = geometryDescriptors[handle];
    globalBufferAllocator.Free(descriptor.dataAlloc);
    geometryDescriptors.FreeRaw(static_cast<int>(handle));
}

void GPUDrivenManager::AllocateForMesh(GpuGeometryDescriptor& descriptor, const Submesh& submesh)
{
    auto vertexByteSize = submesh.GetVertexDataByteSize();
    auto indexByteSize = submesh.GetIndexDataByteSize();

    // The GpuGeometry header struct is stored first so the shader can read it
    // via LoadData<GpuGeometry>(renderData.geometryOffset).
    constexpr uint32_t geometryHeaderSize = sizeof(GpuGeometry);
    // static_assert(sizeof(GpuGeometry) % 16 == 0, "GpuGeometry must be 16-byte aligned");

    ThreadLocalAllocator tempAllocator;
    auto totalSize = geometryHeaderSize + indexByteSize + vertexByteSize;

    uint8_t* staging = (uint8_t*)tempAllocator.allocate(totalSize, globalDataAlignment);
    globalBufferAllocator.Allocate(totalSize, globalDataAlignment, descriptor.dataAlloc);

    const uint32_t* indices = submesh.GetIndices().data();
    const float3* positions = submesh.GetPositions().data();
    const unsigned char* attributes = submesh.GetAttribute().GetData().data();

    // Offsets into globalBuffer for each region (header is at dataAlloc.offset).
    uint32_t sizeOffset = geometryHeaderSize;

    descriptor.geometry.indexCount = submesh.GetIndexCount();
    descriptor.geometry.indexOffset = descriptor.dataAlloc.offset + sizeOffset;
    memcpy(staging + sizeOffset, indices, indexByteSize);
    sizeOffset += indexByteSize;

    uint32_t positionSize = submesh.GetPositions().size() * 3 * sizeof(float);
    descriptor.geometry.positionOffset = descriptor.dataAlloc.offset + sizeOffset;
    memcpy(staging + sizeOffset, positions, positionSize);
    sizeOffset += positionSize;

    uint32_t attributeSize = submesh.GetAttribute().GetSize();
    descriptor.geometry.attributeOffset = descriptor.dataAlloc.offset + sizeOffset;
    memcpy(staging + sizeOffset, attributes, attributeSize);

    descriptor.geometry.attributeFlags = 0;
    descriptor.geometry.normalOffset = 0;
    descriptor.geometry.tangentOffset = 0;
    descriptor.geometry.uvOffset = 0;
    descriptor.geometry.boneOffset = 0;
    descriptor.geometry.colorOffset = 0;

    uint32_t attributeStride = 0;
    for (const auto& attr : submesh.GetAttribute().GetDescription())
    {
        if (attr.semanticIndex == 0)
        {
            if (attr.semanticName == VertexAttributeSemantics::Normal)
            {
                descriptor.geometry.attributeFlags |= GpuGeometry::GetNormalBit();
                descriptor.geometry.normalOffset = attributeStride;
            }
            else if (attr.semanticName == VertexAttributeSemantics::Tangent)
            {
                descriptor.geometry.attributeFlags |= GpuGeometry::GetTangentBit();
                descriptor.geometry.tangentOffset = attributeStride;
            }
            else if (attr.semanticName == VertexAttributeSemantics::Texcoord)
            {
                descriptor.geometry.attributeFlags |= GpuGeometry::GetHasUVBit();
                descriptor.geometry.uvOffset = attributeStride;
            }
            else if (attr.semanticName == VertexAttributeSemantics::Bone)
            {
                descriptor.geometry.attributeFlags |= GpuGeometry::GetBoneBit();
                descriptor.geometry.boneOffset = attributeStride;
            }
            else if (attr.semanticName == VertexAttributeSemantics::Color)
            {
                descriptor.geometry.attributeFlags |= GpuGeometry::GetColorBit();
                descriptor.geometry.colorOffset = attributeStride;
            }
        }

        attributeStride += attr.size;
    }
    descriptor.geometry.attributeStride = attributeStride;

    // Write the fully-populated GpuGeometry header at the start of the block.
    memcpy(staging, &descriptor.geometry, geometryHeaderSize);

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

void GPUDrivenManager::UploadMaterial(GPUMaterialHandle handle)
{
    auto& descriptor = materialDescriptortors[handle];

    GetGfxDriver()->UploadBuffer(
        *globalBuffer,
        reinterpret_cast<uint8_t*>(&descriptor.materialData),
        sizeof(GpuMaterial),
        descriptor.dataAlloc.offset
    );
}

void GPUDrivenManager::UpdateMaterialExtraData(
    GpuMaterialDescriptor& descriptor,
    std::span<const uint8_t> extraData
)
{
    if (extraData.empty())
    {
        globalBufferAllocator.Free(descriptor.extraDataAlloc);
        descriptor.materialData.extraMaterialData = InvalidTextureIndex;
        return;
    }

    if (!descriptor.extraDataAlloc.IsValid() || descriptor.extraDataAlloc.size != extraData.size())
    {
        globalBufferAllocator.Free(descriptor.extraDataAlloc);
        const bool allocated = globalBufferAllocator.Allocate(
            extraData.size(),
            globalDataAlignment,
            descriptor.extraDataAlloc
        );
        ASSERT(allocated && "GPU-driven global buffer is out of space for material extra data");
        if (!allocated)
        {
            descriptor.materialData.extraMaterialData = InvalidTextureIndex;
            return;
        }
    }

    descriptor.materialData.extraMaterialData = static_cast<uint32_t>(descriptor.extraDataAlloc.offset);
    GetGfxDriver()->UploadBuffer(
        *globalBuffer,
        const_cast<uint8_t*>(extraData.data()),
        extraData.size(),
        descriptor.extraDataAlloc.offset
    );
}

GPUMaterialHandle GPUDrivenManager::RegisterMaterial(
    const GpuMaterial& data,
    std::span<const uint8_t> extraData
)
{
    std::lock_guard<std::mutex> lock(mutex);

    GPUMaterialHandle handle = materialDescriptortors.AllocateRaw();
    auto& descriptor = materialDescriptortors[handle];
    globalBufferAllocator.Allocate(sizeof(GpuMaterial), globalDataAlignment, descriptor.dataAlloc);
    descriptor.materialData = data;
    UpdateMaterialExtraData(descriptor, extraData);
    UploadMaterial(handle);

    return handle;
}

void GPUDrivenManager::UpdateMaterial(
    GPUMaterialHandle handle,
    const GpuMaterial& data,
    std::span<const uint8_t> extraData
)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto& descriptor = materialDescriptortors[handle];
    descriptor.materialData = data;
    UpdateMaterialExtraData(descriptor, extraData);
    UploadMaterial(handle);
}

void GPUDrivenManager::UnregisterMaterial(GPUMaterialHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto& descriptor = materialDescriptortors[handle];
    globalBufferAllocator.Free(descriptor.extraDataAlloc);
    globalBufferAllocator.Free(descriptor.dataAlloc);
    materialDescriptortors.FreeRaw(static_cast<int>(handle));
}

void GPUDrivenManager::UploadObject(GpuObjectHandle handle)
{
    auto& descriptor = objectDescriptors[handle];

    GetGfxDriver()->UploadBuffer(
        *globalBuffer,
        reinterpret_cast<uint8_t*>(&descriptor.gpuObject),
        sizeof(GpuObject),
        descriptor.dataAlloc.offset
    );
}

GpuObjectHandle GPUDrivenManager::RegisterObject(
    const float4x4& modell,
    const float4x4& invTspModel,
    GpuRenderDataListHandle renderDataListHandle,
    uint32_t skeletonOffset
)
{
    std::lock_guard<std::mutex> lock(mutex);

    GpuObjectHandle handle = objectDescriptors.AllocateRaw();
    auto& descriptor = objectDescriptors[handle];
    auto& renderDataList = renderDataListDescriptors[renderDataListHandle];
    descriptor.gpuObject = GpuObject{
        modell,
        invTspModel,
        static_cast<uint32_t>(renderDataList.renderDataList.size()),
        static_cast<uint32_t>(renderDataList.dataAlloc.offset),
        skeletonOffset,
    };
    descriptor.renderDataListHandle = renderDataListHandle;
    globalBufferAllocator.Allocate(sizeof(GpuObject), globalDataAlignment, descriptor.dataAlloc);
    UploadObject(handle);

    return handle;
}

void GPUDrivenManager::UpdateObject(GpuObjectHandle handle, const GpuObject& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    objectDescriptors[handle].gpuObject = data;
    UploadObject(handle);
}

void GPUDrivenManager::UnregisterObject(GpuObjectHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    globalBufferAllocator.Free(objectDescriptors[handle].dataAlloc);
    objectDescriptors.FreeRaw(static_cast<int>(handle));
    gpuDrivenConfigDirty = true;
}

// --- Descriptor set passthrough for scene buffers ---

void GPUDrivenManager::SetObjectOffsetBuffer(Gfx::Buffer* buffer)
{
    globalDescriptorSet->SetBuffer("gpuObjectOffsets", buffer);
}

void GPUDrivenManager::SetRTObjectOffsetBuffer(Gfx::Buffer* buffer)
{
    globalDescriptorSet->SetBuffer("rtObjectOffsets", buffer);
}

void GPUDrivenManager::BeginGlobalDynamicBufferFrame()
{
    uint64_t frameIndex = GetGfxDriver()->GetFrameIndex();
    if (globalDynamicBufferFrameIndex == frameIndex)
        return;

    globalDynamicBufferFrameIndex = frameIndex;
    globalDynamicBufferOffset = 0;
}

bool GPUDrivenManager::EnsureGlobalDynamicBufferCapacity(uint32_t requiredSize)
{
    if (requiredSize <= globalDynamicBufferSize)
        return true;

    if (globalDynamicBufferOffset != 0)
    {
        SPDLOG_ERROR(
            "GPUDriven dynamic buffer overflow: required {} bytes, capacity {} bytes. Increase capacity or reduce same-frame dynamic data.",
            requiredSize,
            globalDynamicBufferSize
        );
        return false;
    }

    uint32_t newCapacity = globalDynamicBufferSize == 0 ? 64 * 1024 * 1024 : globalDynamicBufferSize;
    while (newCapacity < requiredSize)
        newCapacity *= 2;

    globalDynamicBuffer = GetGfxDriver()->CreateBuffer(
        newCapacity,
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        false,
        false,
        "GPUDrivenGlobalDynamicBuffer"
    );
    globalDynamicBufferSize = newCapacity;
    globalDescriptorSet->SetBuffer("globalDynamicBuffer", globalDynamicBuffer.get());
    return true;
}

GpuDynamicDataAllocation GPUDrivenManager::UploadDynamicData(
    Gfx::CommandBuffer& cmd,
    const void* data,
    uint32_t size,
    uint32_t alignment
)
{
    BeginGlobalDynamicBufferFrame();

    if (data == nullptr || size == 0)
        return {};

    if (alignment == 0)
        alignment = 1;

    uint32_t alignedOffset = ((globalDynamicBufferOffset + alignment - 1) / alignment) * alignment;
    uint32_t requiredSize = alignedOffset + size;
    if (!EnsureGlobalDynamicBufferCapacity(requiredSize))
        return {};

    cmd.UploadData(
        *globalDynamicBuffer,
        const_cast<void*>(data),
        size,
        alignedOffset
    );

    globalDynamicBufferOffset = requiredSize;
    return {alignedOffset, size};
}

void GPUDrivenManager::BeginIndirectArenaFrame()
{
    uint64_t frameIndex = GetGfxDriver()->GetFrameIndex();
    if (indirectArenaFrameIndex == frameIndex)
    {
        return;
    }

    indirectArenaFrameIndex = frameIndex;
    indirectCommandBufferOffset = 0;
}

bool GPUDrivenManager::EnsureIndirectCommandCapacity(uint32_t requiredSize)
{
    if (requiredSize <= indirectCommandBufferCapacity)
        return true;

    if (indirectCommandBufferOffset != 0)
    {
        SPDLOG_ERROR(
            "GPUDriven indirect arena overflow: required {} draws, capacity {}. Increase arena slack or reduce same-frame preview renders.",
            requiredSize,
            indirectCommandBufferCapacity
        );
        return false;
    }

    uint32_t newCapacity = indirectCommandBufferCapacity == 0 ? 8192 : indirectCommandBufferCapacity;
    while (newCapacity < requiredSize)
        newCapacity *= 2;
    newCapacity *= 2;

    indirectCommandBuffer = GetGfxDriver()->CreateBuffer(
        newCapacity * sizeof(DrawIndexedIndirectCommand),
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Indirect | Gfx::BufferUsage::Transfer_Dst,
        false,
        false,
        "GPUDrivenIndirectCommands"
    );

    indirectCommandExtraBuffer = GetGfxDriver()->CreateBuffer(
        newCapacity * sizeof(GpuDrawExtra),
        Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
        false,
        false,
        "GPUDrivenIndirectCommands Extra"
    );

    indirectCommandBufferCapacity = newCapacity;
    SetObjectOffsetBuffer(indirectCommandExtraBuffer.get());
    return true;
}

IndirectDrawData GPUDrivenManager::UploadIndirectDrawData(
    Gfx::CommandBuffer& cmd,
    std::span<const DrawIndexedIndirectCommand> commands,
    std::span<const GpuDrawExtra> drawExtras
)
{
    BeginIndirectArenaFrame();

    if (commands.empty())
        return {};

    uint32_t firstDrawIndex = indirectCommandBufferOffset;
    uint32_t requiredSize = firstDrawIndex + static_cast<uint32_t>(commands.size());
    if (!EnsureIndirectCommandCapacity(requiredSize))
    {
        return {};
    }

    SetObjectOffsetBuffer(indirectCommandExtraBuffer.get());
    cmd.UploadData(
        *indirectCommandBuffer,
        const_cast<DrawIndexedIndirectCommand*>(commands.data()),
        commands.size() * sizeof(DrawIndexedIndirectCommand),
        firstDrawIndex * sizeof(DrawIndexedIndirectCommand)
    );
    cmd.UploadData(
        *indirectCommandExtraBuffer,
        const_cast<GpuDrawExtra*>(drawExtras.data()),
        drawExtras.size() * sizeof(GpuDrawExtra),
        firstDrawIndex * sizeof(GpuDrawExtra)
    );

    indirectCommandBufferOffset = requiredSize;
    return {indirectCommandBuffer.get(), firstDrawIndex, static_cast<uint32_t>(commands.size())};
}

// --- Lifecycle ---

GPUDrivenManager& GPUDrivenManager::Instance()
{
    return *GetInstanceInternal();
}

GPUDrivenManager* GPUDrivenManager::TryGetInstance()
{
    return GetInstanceInternal().get();
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

uint64_t GPUDrivenManager::GetGlobalBufferShaderDeviceAddress()
{
    return globalBuffer->GetShaderDeviceAddress();
}
} // namespace Rendering
