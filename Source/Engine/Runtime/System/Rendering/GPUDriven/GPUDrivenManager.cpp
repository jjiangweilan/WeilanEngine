#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/ResourceHandle.hpp"
#include "Engine/Library/Allocators/ThreadLocalAllocator.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/GPUParameter.hpp"

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

    // Create global descriptor set (set 0)
    globalDescriptorSet = GetGfxDriver()->CreateShaderResource();
    globalDescriptorSet->SetBuffer("globalBuffer", globalBuffer.get());

    // Create global sampler table matching PerScene.hlsl globalSamplers[10].
    // Layout: index = addressMode * 2 + filterMode
    //   addressMode: Repeat=0, MirroredRepeat=1, ClampToEdge=2, ClampToBorder=3
    //   filterMode:  Nearest=0, Linear=1
    //   Indices 8 and 9 are fallbacks for MirrorClampToEdge (unsupported), mapped to ClampToEdge.
    using AM = Gfx::SamplerAddressMode;
    using FM = Gfx::FilterMode;
    struct SamplerDesc
    {
        AM addr;
        FM filter;
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
    };
    for (int i = 0; i < GlobalSamplerCount; ++i)
    {
        Gfx::Sampler::CreateInfo ci{};
        ci.addressModeU = descs[i].addr;
        ci.addressModeV = descs[i].addr;
        ci.addressModeW = descs[i].addr;
        ci.minFilter = descs[i].filter;
        ci.magFilter = descs[i].filter;
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

    uint32_t attributeStride = 0;
    descriptor.geometry.attributeFlags = 0;
    VertexAttributes::Attribute attr;
    if (submesh.GetAttribute().FindSemantics(VertexAttributeSemantics::Normal, 0, attr))
    {
        // If the mesh doesn't have normals, we can compute them on the fly in the shader.
        descriptor.geometry.attributeFlags |= GpuGeometry::GetNormalBit();
        attributeStride += attr.size;
    }
    if (submesh.GetAttribute().FindSemantics(VertexAttributeSemantics::Tangent, 0, attr))
    {
        // If the mesh doesn't have tangents, we can compute them on the fly in the shader.
        descriptor.geometry.attributeFlags |= GpuGeometry::GetTangentBit();
        attributeStride += attr.size;
    }
    if (submesh.GetAttribute().FindSemantics(VertexAttributeSemantics::Texcoord, 0, attr))
    {
        // If the mesh doesn't have UVs, we can use a default value in the shader.
        descriptor.geometry.attributeFlags |= GpuGeometry::GetHasUVBit();
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

GPUMaterialHandle GPUDrivenManager::RegisterMaterial(const GpuMaterial& data)
{
    std::lock_guard<std::mutex> lock(mutex);

    GPUMaterialHandle handle = materialDescriptortors.AllocateRaw();
    globalBufferAllocator.Allocate(sizeof(GpuMaterial), globalDataAlignment, materialDescriptortors[handle].dataAlloc);
    materialDescriptortors[handle].materialData = data;
    UploadMaterial(handle);

    return handle;
}

void GPUDrivenManager::UpdateMaterial(GPUMaterialHandle handle, const GpuMaterial& data)
{
    std::lock_guard<std::mutex> lock(mutex);
    materialDescriptortors[handle].materialData = data;
    UploadMaterial(handle);
}

void GPUDrivenManager::UnregisterMaterial(GPUMaterialHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex);
    globalBufferAllocator.Free(materialDescriptortors[handle].dataAlloc);
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
    GpuRenderDataListHandle renderDataListHandle
)
{
    std::lock_guard<std::mutex> lock(mutex);

    GpuObjectHandle handle = objectDescriptors.AllocateRaw();
    auto& descriptor = objectDescriptors[handle];
    auto& renderDataList = renderDataListDescriptors[renderDataListHandle];
    descriptor.gpuObject = GpuObject{modell, invTspModel, static_cast<uint32_t>(renderDataList.renderDataList.size()), static_cast<uint32_t>(renderDataList.dataAlloc.offset)};
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

uint64_t GPUDrivenManager::GetGlobalBufferShaderDeviceAddress()
{
    return globalBuffer->GetShaderDeviceAddress();
}
} // namespace Rendering
