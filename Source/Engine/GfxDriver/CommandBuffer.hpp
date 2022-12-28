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

namespace Engine
{
enum class IndexBufferType
{
    UInt16,
    UInt32
};

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
    Gfx::ImageSubresourceRange range;
    Offset3D offset;
    Extent3D extend;
};

class CommandBuffer
{
public:
    virtual ~CommandBuffer(){};
    virtual void BindResource(RefPtr<Gfx::ShaderResource> resource) = 0;
    virtual void BindVertexBuffer(const std::vector<RefPtr<Gfx::Buffer>>& buffers, const std::vector<uint64_t>& offsets,
                                  uint32_t firstBindingIndex) = 0;
    virtual void BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, IndexBufferType indexBufferType) = 0;
    virtual void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const Gfx::ShaderConfig& config) = 0;

    virtual void BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass,
                                 const std::vector<Gfx::ClearValue>& clearValues) = 0;
    virtual void NextRenderPass() = 0;
    virtual void EndRenderPass() = 0;

    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset,
                             uint32_t firstInstance) = 0;
    virtual void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;

    virtual void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) = 0;
    virtual void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) = 0;
    virtual void CopyBuffer(RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst,
                            const std::vector<BufferCopyRegion>& copyRegions) = 0;
    virtual void CopyBufferToImage(RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst,
                                   const std::vector<BufferImageCopyRegion>& regions) = 0;
    virtual void Barrier(GPUBarrier* barriers, uint32_t barrierCount) = 0;
    virtual void Begin() = 0;
    virtual void End() = 0;
};
} // namespace Engine
