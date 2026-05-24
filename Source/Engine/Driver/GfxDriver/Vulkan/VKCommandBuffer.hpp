#pragma once
#include "../CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKShaderResource.hpp"
#include "Engine/Library/DynamicArray.hpp"
#include "VKRenderPass.hpp"
#include <list>

namespace Gfx
{

class VKCommandBufferProcessor;
class VKShaderResource;
class VKShaderProgram;
class VKDevice;

struct VKAsyncReadbackHandle : public AsyncReadbackHandle
{
    uint8_t* GetData() override { return nullptr; }
    bool IsComplete() override { return isComplete; }

    std::vector<std::uint8_t> data;
    std::atomic_bool isComplete;
};

struct VKSetClearValuesCmd
{
    std::vector<Gfx::ClearValue> clearValues;
};

struct VKDrawIndexedCmd
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;
};

struct VKDrawIndirectCmd
{
    BufferIdentifier buffer;
    size_t offset;
    uint32_t drawCount;
    uint32_t stride;
};

struct VKDrawIndexedIndirectCmd
{
    BufferIdentifier buffer;
    size_t offset;
    uint32_t drawCount;
    uint32_t stride;
};

struct VKDrawCmd
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct VKBeginRenderPassCmd
{
    VKRenderPass* renderPass;
    VkClearValue clearValues[8];
    int clearValueCount;

    int barrierOffset;
    int barrierCount;
};

struct VKSetLineWidthCmd
{
    float lineWidth;
};

struct VKSetDepthBiasCmd
{
    float constantFactor;
    float clamp;
    float slopeFactor;
};

struct VKSetDepthBiasEnableCmd
{
    bool enable;
};

struct VKRGBeginRenderPassCmd
{
    RenderPass renderPass;
    VkClearValue clearValues[8];
    int clearValueCount;

    int barrierOffset;
    int barrierCount;
};

struct VKDynamicRenderPassCmd
{
    std::vector<RenderAttachment> imageIdentifiers;
    std::vector<ClearValue> clearValues;

    VKRenderPass* resolvedRenderPass;
    int barrierOffset;
    int barrierCount;
};

struct VKEndRenderPassCmd
{};

struct VKBindResourceCmd
{
    uint32_t set;
    VKShaderResource* resource;
};

struct VKBindShaderProgramCmd
{
    VKShaderProgram* program;
    PipelineConfig config;
};

struct VKBindVertexBufferCmd
{
    uint32_t firstBindingIndex;
    VertexBufferBinding vertexBufferBindings[8];
    uint32_t vertexBufferBindingCount;
};

struct VKBindIndexBufferCmd
{
    BufferIdentifier buffer;
    uint64_t offset;
    VkIndexType indexType;
};

struct VKSetTextureCmd
{
    ShaderBindingHandle handle;
    Gfx::Image* image;
    std::optional<ImageViewOption> imageViewOption;
    int index;
};

struct VKSetBufferCmd
{
    ShaderBindingHandle handle;
    BufferIdentifier buffer;
    int index;
};

struct VKSetViewportCmd
{
    VkViewport viewport;
};

struct VKCopyImageToBufferCmd
{
    VKImage* src;
    BufferIdentifier dst;
    BufferImageCopyRegion regions[8];
    int regionsCount;

    int barrierOffset;
    int barrierCount;
};

struct VKSetPushConstantCmd
{
    VKShaderProgram* shaderProgram;
    VkShaderStageFlags stages;
    uint32_t dataSize;
    uint8_t data[128];
};

struct VKSetScissorCmd
{
    uint32_t firstScissor;
    uint32_t scissorCount;
    VkRect2D rects[8];
};

struct VKDispatchCmd
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;

    int barrierOffset;
    int barrierCount;
};

struct VKDispatchIndirectCmd
{
    BufferIdentifier buffer;
    size_t bufferOffset;

    int barrierOffset;
    int barrierCount;
};

struct VKNextRenderPassCmd
{};

struct VKPushDescriptorCmd
{
    VKShaderProgram* shader;
    uint32_t set;
    uint32_t bindingCount;
    DescriptorBinding bindings[8];
};

struct VKCopyBufferCmd
{
    BufferIdentifier src;
    BufferIdentifier dst;
    uint32_t copyRegionCount;
    VkBufferCopy copyRegions[8];

    int barrierOffset;
    int barrierCount;
};

struct VKUploadDataCmd
{
    std::vector<uint8_t> data;
    BufferIdentifier dst;
    size_t dstOffset;

    int barrierOffset;
    int barrierCount;
};

struct VKCopyBufferToImageCmd
{
    BufferIdentifier src;
    VKImage* dst;
    uint32_t regionCount;
    VkBufferImageCopy regions[8];

    int barrierOffset;
    int barrierCount;
};

struct VKDynamicBindResourceCmd
{
    uint32_t set;
    std::vector<DynamicBinding> bindings;
};

struct VKBlitCmd
{
    VKImage* from;
    VKImage* to;
    BlitOp blitOp;

    int barrierOffset;
    int barrierCount;
};

struct VKPresentCmd
{
    VKImage* image;

    int barrierOffset;
    int barrierCount;
};

struct VKBeginLabelCmd
{
    std::string label;
    float color[4];
};

struct VKEndLabelCmd
{};

struct VKInsertLabelCmd
{
    std::string label;
    float color[4];
};

struct VKAllocateAttachmentCmd
{
    ImageIdentifier* id;
    RenderImageDescriptor desc;
};

struct VKAsyncReadbackCmd
{
    BufferIdentifier buffer;
    size_t size;
    size_t offset;

    std::shared_ptr<AsyncReadbackHandle>* handle;
};

struct VKBuildBLASCmd
{
    RayTracingMeshHandle handle;
    std::vector<VkAccelerationStructureGeometryKHR> vkGeometries;
    std::vector<uint32_t> maxPrimitiveCounts;
};

struct VKBuildTLASCmd
{
    RayTracingSceneHandle handle;
    std::vector<RayTracingInstanceHandle> instances;
};

struct VKGraphicsBlitCmd
{
    ImageIdentifier from;
    ImageIdentifier to;
};

struct VKClearColorImageCmd
{
    Image* image;
    ClearColor clearValue;

    int barrierOffset;
    int barrierCount;
};

struct VKAllocateBufferCmd
{
    TemporaryBufferHandle handle;
    uint64_t size = 0;
    uint64_t alignment = 16;
    TemporaryBufferUsage usage = TemporaryBufferUsage::Storage;
};

struct VKNoneCmd
{};

enum class VKCmdType
{
    None,
    DrawIndexed,
    DrawIndexedIndirect,
    DrawIndirect,
    Draw,
    BeginRenderPass,
    RGBeginRenderPass,
    DynamicBeginRenderPass,
    DynamicBindResource,
    EndRenderPass,
    Blit,
    BindResource,
    BindVertexBuffer,
    BindShaderProgram,
    BindIndexBuffer,
    SetViewport,
    CopyImageToBuffer,
    SetPushConstant,
    SetScissor,
    Dispatch,
    DispatchIndirect,
    NextRenderPass,
    PushDescriptorSet,
    CopyBuffer,
    CopyBufferToImage,
    SetBuffer,
    SetTexture,
    AllocateAttachment,
    Present,
    SetLineWidth,
    SetDepthBias,
    SetDepthBiasEnable,
    BeginLabel,
    EndLabel,
    InsertLabel,
    AsyncReadback,
    BuildBLAS,
    BuildTLAS,
    GraphicsBlit,
    ClearColorImage,
    UploadData,
    AllocateBuffer,
};

struct VKCmd
{
    VKCmdType type;
    std::variant<
        VKNoneCmd,
        VKDrawIndexedCmd,
        VKDrawIndexedIndirectCmd,
        VKDrawIndirectCmd,
        VKDrawCmd,
        VKBeginRenderPassCmd,
        VKRGBeginRenderPassCmd,
        VKDynamicRenderPassCmd,
        VKDynamicBindResourceCmd,
        VKEndRenderPassCmd,
        VKBlitCmd,
        VKBindResourceCmd,
        VKBindVertexBufferCmd,
        VKBindShaderProgramCmd,
        VKBindIndexBufferCmd,
        VKSetViewportCmd,
        VKCopyImageToBufferCmd,
        VKSetPushConstantCmd,
        VKSetScissorCmd,
        VKDispatchCmd,
        VKDispatchIndirectCmd,
        VKNextRenderPassCmd,
        VKPushDescriptorCmd,
        VKCopyBufferCmd,
        VKCopyBufferToImageCmd,
        VKSetBufferCmd,
        VKSetTextureCmd,
        VKAllocateAttachmentCmd,
        VKPresentCmd,
        VKSetLineWidthCmd,
        VKSetDepthBiasCmd,
        VKSetDepthBiasEnableCmd,
        VKBeginLabelCmd,
        VKEndLabelCmd,
        VKInsertLabelCmd,
        VKAsyncReadbackCmd,
        VKBuildBLASCmd,
        VKBuildTLASCmd,
        VKGraphicsBlitCmd,
        VKClearColorImageCmd,
        VKUploadDataCmd,
        VKAllocateBufferCmd>
        args;
};

class VKCommandBuffer : public CommandBuffer
{
public:
    VKCommandBuffer(VKCommandBufferProcessor* graph)
        : graph(graph) {}
    VKCommandBuffer(const VKCommandBuffer& other) = delete;
    ~VKCommandBuffer() {};

    void BeginLabel(std::string_view label, float color[4]) override;
    void BeginLabel(std::string_view label, const glm::float4& color) override;
    void EndLabel() override;
    void InsertLabel(std::string_view label, float color[4]) override;
    void InsertLabel(std::string_view label, const glm::float4& color) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexed(
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
    ) override;
    void DrawIndirect(
        BufferIdentifier buffer, size_t offset, uint32_t drawCount, uint32_t stride
    ) override;
    void DrawIndexedIndirect(
        BufferIdentifier buffer, size_t offset, uint32_t drawCount, uint32_t stride
    ) override;

    void BeginRenderPass(std::span<const RenderAttachment> images, std::span<ClearValue> clearValues) override;

    void BeginRenderPass(Gfx::RenderPass_Deprecated& renderPass, std::span<ClearValue> clearValues) override;
    void EndRenderPass() override;
    void ClearColorImage(Image* image, const ClearColor& color) override;

    void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp = {}) override;
    void GraphicsBlit(const ImageIdentifier& from, const ImageIdentifier& to) override;
    void BindResource(uint32_t set, Gfx::ShaderResource* resource) override;
    void BindResource(uint32_t set, const std::vector<DynamicBinding>& bindings) override;
    void BindVertexBuffer(
        std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
    ) override;
    void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const PipelineConfig& config) override;
    void BindIndexBuffer(
        BufferIdentifier buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType
    ) override;

    void SetViewport(const Viewport& viewport) override;
    void CopyImageToBuffer(
        RefPtr<Gfx::Image> src, BufferIdentifier dst, std::span<BufferImageCopyRegion> regions
    ) override;
    void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) override;
    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void DispatchIndirect(BufferIdentifier buffer, size_t bufferOffset) override;
    void NextRenderPass() override;
    void PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings) override;

    void CopyBuffer(
        BufferIdentifier bSrc, BufferIdentifier bDst, std::span<BufferCopyRegion> copyRegions
    ) override;
    void CopyBufferToImage(
        BufferIdentifier src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
    ) override;
    void Begin() override {}
    void End() override {}

    void Blit(ImageIdentifier src, ImageIdentifier dst, BlitOp blitOp) override;
    void SetTexture(
        ShaderBindingHandle handle, int index, ImageIdentifier id, std::optional<ImageViewOption> imageViewOption
    ) override;
    void SetTexture(
        ShaderBindingHandle handle, int index, Gfx::Image& image, std::optional<ImageViewOption> imageViewOption
    ) override;
    void SetBuffer(ShaderBindingHandle handle, int index, BufferIdentifier buffer) override;

    void AllocateAttachment(const ImageIdentifier& id, RenderImageDescriptor& desc) override;

    TemporaryBufferHandle AllocateBuffer(
        size_t size,
        TemporaryBufferUsage usage,
        size_t alignment = 16
    ) override;

    void BeginRenderPass(RenderPass& renderPass, std::span<ClearValue> clearValues) override;
    void SetLineWidth(float lineWidth) override;
    void SetDepthBias(float constantFactor, float clamp, float slopeFactor) override;
    void SetDepthBiasEnable(bool enable) override;

    void PresentImage(VKImage* image);

    std::shared_ptr<AsyncReadbackHandle> AsyncReadback(
        BufferIdentifier buffer, size_t size, size_t offset
    ) override;

    void UploadData(
        BufferIdentifier buffer, void* data, size_t dataSize, size_t offset = 0
    ) override;

    void BuildBLAS(RayTracingMeshHandle handle, std::span<VkAccelerationStructureGeometryKHR> geometries, std::span<uint32_t> maxPrimitiveCounts);
    void BuildTLAS(RayTracingSceneHandle handle, std::span<RayTracingInstanceHandle> instances);

    void Reset(bool releaseResource) override
    {
        readbacks.clear();
        cmds.clear();
    }

    std::span<VKCmd> GetCmds() { return cmds; }

private:
    bool validationCheck = true;
    bool beginLabelStarted = false;
    std::string currentLabel;

    std::vector<VKCmd> cmds;
    VKCommandBufferProcessor* graph;
    std::list<std::shared_ptr<AsyncReadbackHandle>> readbacks;

    friend struct VKFramePrepareData;
};
} // namespace Gfx
