#pragma once
#include "../CommandBuffer.hpp"
#include "GfxDriver/Vulkan/VKShaderResource.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Libs/LinearAllocator.hpp"
#include "VKRenderPass.hpp"
#include <list>
#include <vector>
#include <vulkan/vulkan.h>

namespace Gfx
{
namespace VK::RenderGraph
{
class Graph;
}

class VKShaderResource;
class VKShaderProgram;
class VKDevice;

struct VKDrawIndexedCmd
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;

    void operator()(VkCommandBuffer cmd)
    {
        vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }
};

struct VKDrawCmd
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;

    void operator()(VkCommandBuffer cmd)
    {
        vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
    }
};

struct VKBeginRenderPassCmd
{
    VKRenderPass* renderPass;
    VkClearValue clearValues[8];
    int clearValueCount;

    // used in VKRenderGraph
    int barrierOffset;
    int barrierCount;
};

struct VKSetLineWidthCmd
{
    float lineWidth;
};

struct VKRGBeginRenderPassCmd
{
    RG::RenderPass* renderPass;
    VkClearValue clearValues[8];
    int clearValueCount;

    // used in VKRenderGraph
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
    const ShaderConfig* config;
};

struct VKBindVertexBufferCmd
{
    uint32_t firstBindingIndex;
    VertexBufferBinding vertexBufferBindings[8];
    uint32_t vertexBufferBindingCount;
};

struct VKBindIndexBufferCmd
{
    VKBuffer* buffer;
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
    VKBuffer* buffer;
    int index;
};

struct VKSetViewportCmd
{
    VkViewport viewport;
};

struct VKCopyImageToBufferCmd
{
    VKImage* src;
    VKBuffer* dst;
    BufferImageCopyRegion regions[8];
    int regionsCount;

    // used in VKRenderGraph
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

    // used in VKRenderGraph
    int barrierOffset;
    int barrierCount;
};

struct VKDispatchIndirectCmd
{
    VKBuffer* buffer;
    size_t bufferOffset;

    // used in VKRenderGraph
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
    VKBuffer* src;
    VKBuffer* dst;
    uint32_t copyRegionCount;
    VkBufferCopy copyRegions[8];

    // used in VKRenderGraph
    int barrierOffset;
    int barrierCount;
};

struct VKCopyBufferToImageCmd
{
    VKBuffer* src;
    VKImage* dst;
    uint32_t regionCount;
    VkBufferImageCopy regions[8];

    // used in VKRenderGraph
    int barrierOffset;
    int barrierCount;
};

struct VKBlitCmd
{
    VKImage* from;
    VKImage* to;
    BlitOp blitOp;

    // used in VKRenderGraph
    int barrierOffset;
    int barrierCount;
};

struct VKPresentCmd
{
    VKImage* image;

    // used in VKRenderGraph
    int barrierOffset;
    int barrierCount;
};

struct VKBeginLabelCmd
{
    char* label;
    float color[4];
};

struct VKEndLabelCmd
{};

struct VKInsetLabelCmd
{
    char* label;
    float color[4];
};

struct VKAllocateAttachmentCmd
{
    RG::ImageIdentifier* id;
    RG::ImageDescription desc;
};

struct VKAsyncReadback
{
    Gfx::Buffer* buffer;
    void* dst;
    size_t size;
    size_t offset;

    // the readback handle is temporarily stored in the command buffer, the owner ship will be moved to VKRenderGraph
    // later
    std::shared_ptr<AsyncReadbackHandle>* handle;
};

enum class VKCmdType
{
    None,
    DrawIndexed,
    Draw,
    BeginRenderPass,
    RGBeginRenderPass,
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
    DispatchIndir,
    NextRenderPass,
    PushDescriptorSet,
    CopyBuffer,
    CopyBufferToImage,
    SetBuffer,
    SetTexture,
    AllocateAttachment,
    Present,
    SetLineWidth,
    BeginLabel,
    EndLabel,
    InsertLabel,
    AsyncReadback
};

struct VKCmd
{
    VKCmdType type;
    union
    {
        VKDrawIndexedCmd drawIndexed;
        VKDrawCmd draw;
        VKBeginRenderPassCmd beginRenderPass;
        VKRGBeginRenderPassCmd rgBeginRenderPass;
        VKEndRenderPassCmd endRenderPass;
        VKBlitCmd blit;
        VKBindResourceCmd bindResource;
        VKBindShaderProgramCmd bindShaderProgram;
        VKBindVertexBufferCmd bindVertexBuffer;
        VKBindIndexBufferCmd bindIndexBuffer;

        VKSetLineWidthCmd setLineWidth;
        VKAsyncReadback asyncReadback;
        VKSetViewportCmd setViewport;
        VKCopyImageToBufferCmd copyImageToBuffer;
        VKSetPushConstantCmd setPushConstant;
        VKSetScissorCmd setScissor;
        VKDispatchCmd dispatch;
        VKDispatchIndirectCmd dispatchIndir;
        VKNextRenderPassCmd nextRenderPass;
        VKPushDescriptorCmd pushDescriptor;
        VKCopyBufferCmd copyBuffer;
        VKCopyBufferToImageCmd copyBufferToImage;

        VKSetTextureCmd setTexture;
        VKSetBufferCmd setBuffer;
        VKPresentCmd present;

        VKBeginLabelCmd beginLabel;
        VKEndLabelCmd endLabel;
        VKInsetLabelCmd insertLabel;

        VKAllocateAttachmentCmd allocateAttachment;
    };
};

class VKCommandBuffer : public CommandBuffer
{
public:
    VKCommandBuffer(VK::RenderGraph::Graph* graph) : graph(graph) {}
    VKCommandBuffer(const VKCommandBuffer& other) = delete;
    ~VKCommandBuffer() {};

    void BeginLabel(std::string_view label, float color[4]) override;
    void EndLabel() override;
    void InsertLabel(std::string_view label, float color[4]) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexed(
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
    ) override;
    void BeginRenderPass(Gfx::RenderPass& renderPass, std::span<ClearValue> clearValues) override;
    void EndRenderPass() override;

    void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp = {}) override;
    // renderpass and framebuffer have to be compatible.
    // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
    // void BindResource(RefPtr<Gfx::ShaderResource> resource) override;
    void BindResource(uint32_t set, Gfx::ShaderResource* resource) override;
    void BindVertexBuffer(std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex)
        override;
    void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const ShaderConfig& config) override;
    void BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType) override;

    void SetViewport(const Viewport& viewport) override;
    void CopyImageToBuffer(RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions)
        override;
    void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) override;
    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void DispatchIndir(Buffer* buffer, size_t bufferOffset) override;
    void NextRenderPass() override;
    void PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings) override;

    void CopyBuffer(RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions)
        override;
    void CopyBufferToImage(RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions)
        override;
    void Begin() override {}
    void End() override {}

    void Blit(RG::ImageIdentifier src, RG::ImageIdentifier dst, BlitOp blitOp) override;
    void SetTexture(
        ShaderBindingHandle handle, int index, RG::ImageIdentifier id, std::optional<ImageViewOption> imageViewOption
    ) override;
    void SetTexture(
        ShaderBindingHandle handle, int index, Gfx::Image& image, std::optional<ImageViewOption> imageViewOption
    ) override;
    void SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer& buffer) override;

    void AllocateAttachment(RG::ImageIdentifier& id, RG::ImageDescription& desc) override;
    void BeginRenderPass(RG::RenderPass& renderPass, std::span<ClearValue> clearValues) override;
    void SetLineWidth(float lineWidth) override;

    void PresentImage(VKImage* image);

    std::shared_ptr<AsyncReadbackHandle> AsyncReadback(Gfx::Buffer& buffer, void* dst, size_t size, size_t offset = 0)
        override;

    void Reset(bool releaseResource) override
    {
        readbacks.clear();
        cmds.clear();
        tmpMemory.Reset();
    }

    std::span<VKCmd> GetCmds()
    {
        return cmds;
    }

private:
    std::vector<VKCmd> cmds;
    LinearAllocator<1024> tmpMemory;
    VK::RenderGraph::Graph* graph;
    std::list<std::shared_ptr<AsyncReadbackHandle>> readbacks;
};
} // namespace Gfx
