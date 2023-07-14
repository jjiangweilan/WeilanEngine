#pragma once

#include "Buffer.hpp"
#include "FrameBuffer.hpp"
#include "GfxEnums.hpp"
#include "Image.hpp"
#include "RenderPass.hpp"
#include "ShaderConfig.hpp"
#include "ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
#include <span>
namespace Engine::Gfx
{
enum class CommandBufferType
{
    Primary,
    Secondary
};

#define GFX_QUEUE_FAMILY_IGNORED ~0U
#define GFX_WHOLE_SIZE ~0ULL
// source stage/accessMask/queueFamilyIndex/imageLayout is tracked by
// buffer/image itself the tracking is done by driver
struct GPUBarrier
{
    RefPtr<Gfx::Buffer> buffer = nullptr;
    RefPtr<Gfx::Image> image = nullptr;
    Gfx::PipelineStageFlags srcStageMask;
    Gfx::PipelineStageFlags dstStageMask;
    Gfx::AccessMaskFlags srcAccessMask;
    Gfx::AccessMaskFlags dstAccessMask;

    struct BufferInfo
    {
        uint32_t srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        uint32_t dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        uint64_t offset = 0;
        uint64_t size = GFX_WHOLE_SIZE;
    } bufferInfo;

    struct ImageInfo
    {
        uint32_t srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        uint32_t dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        Gfx::ImageLayout oldLayout;
        Gfx::ImageLayout newLayout;
        Gfx::ImageSubresourceRange subresourceRange;
    } imageInfo;
};

struct BufferCopyRegion
{
    uint64_t srcOffset;
    uint64_t dstOffset;
    uint64_t size;
};

struct BufferImageCopyRegion
{
    uint64_t srcOffset;
    Gfx::ImageSubresourceLayers layers;
    Offset3D offset;
    Extent3D extend;
};

struct VertexBufferBinding
{
    Gfx::Buffer* buffer;
    uint64_t offset;
};

struct Viewport
{
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};

struct DescriptorBinding
{
    uint32_t dstBinding;
    uint32_t dstArrayElement;
    uint32_t descriptorCount;
    Image* image;
    Buffer* buffer;
};

class CommandBuffer
{
public:
    virtual ~CommandBuffer(){};
    virtual void BindResource(RefPtr<Gfx::ShaderResource> resource) = 0;
    virtual void BindVertexBuffer(
        std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
    ) = 0;
    virtual void BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType) = 0;
    virtual void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const Gfx::ShaderConfig& config) = 0;

    virtual void BeginRenderPass(
        RefPtr<Gfx::RenderPass> renderPass, const std::vector<Gfx::ClearValue>& clearValues
    ) = 0;
    virtual void NextRenderPass() = 0;
    virtual void EndRenderPass() = 0;

    virtual void DrawIndexed(
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
    ) = 0;
    virtual void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;

    virtual void PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings) = 0;
    virtual void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) = 0;
    virtual void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) = 0;
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void CopyBuffer(
        RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions
    ) = 0;
    virtual void CopyImageToBuffer(
        RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions
    ) = 0;
    virtual void CopyBufferToImage(
        RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
    ) = 0;
    virtual void Barrier(GPUBarrier* barriers, uint32_t barrierCount) = 0;
    virtual void Begin() = 0;
    virtual void End() = 0;
};
} // namespace Engine::Gfx
