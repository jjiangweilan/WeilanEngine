#pragma once
#include "../CommandBuffer.hpp"
#include "GfxDriver/Vulkan/VKShaderResource.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Libs/LinearAllocator.hpp"
#include "VKRenderPass.hpp"
#include <vector>
#include <vulkan/vulkan.h>

namespace Gfx
{
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
    ResourceHandle handle;
    Gfx::Image* image;
};

struct VKSetUniformBufferCmd
{
    ResourceHandle handle;
    VKBuffer* buffer;
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
};

struct VKDispatchIndirectCmd
{
    VKBuffer* buffer;
    size_t bufferOffset;
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

enum class VKCmdType
{
    None,
    DrawIndexed,
    Draw,
    BeginRenderPass,
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
    SetUniformBuffer,
    SetTexture,
    Present,
};
struct VKCmd
{
    VKCmdType type;
    union
    {
        VKDrawIndexedCmd drawIndexed;
        VKDrawCmd draw;
        VKBeginRenderPassCmd beginRenderPass;
        VKEndRenderPassCmd endRenderPass;
        VKBlitCmd blit;
        VKBindResourceCmd bindResource;
        VKBindShaderProgramCmd bindShaderProgram;
        VKBindVertexBufferCmd bindVertexBuffer;
        VKBindIndexBufferCmd bindIndexBuffer;

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
        VKSetUniformBufferCmd setUniformBuffer;
        VKPresentCmd present;
    };
};
class VKCommandBuffer2 : public CommandBuffer
{
public:
    VKCommandBuffer2() {}
    VKCommandBuffer2(const VKCommandBuffer2& other) = delete;
    ~VKCommandBuffer2(){};

    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexed(
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
    ) override;
    void BeginRenderPass(Gfx::RenderPass& renderPass, const std::vector<Gfx::ClearValue>& clearValues) override;
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

    void SetTexture(ResourceHandle handle, Gfx::Image& image) override;
    void SetUniformBuffer(ResourceHandle handle, Gfx::Buffer& buffer) override;

    void AllocateAttachmentRT(ResourceHandle& handle, const ImageDescription& desc) override{};
    void BeginRenderPass(RG::RenderPass& renderPass, std::span<ClearValue> clearValues) override{};

    void PresentImage(VKImage* image);

    void Reset(bool releaseResource) override
    {
        cmds.clear();
    }

    void Execute(VkCommandBuffer cmd);

    std::span<VKCmd> GetCmds()
    {
        return cmds;
    }

private:
    std::vector<VKCmd> cmds;
    LinearAllocator<32 * 1024> tempMemory;
};
} // namespace Gfx
