#pragma once

#include "Buffer.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "FrameBuffer.hpp"
#include "GfxEnums.hpp"
#include "Image.hpp"
#include "RenderGraph.hpp"
#include "RenderPass.hpp"
#include "ShaderConfig.hpp"
#include "ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
#include <span>
namespace Gfx
{

enum class CommandBufferType
{
    Primary,
    Secondary
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
    ImageView* imageView;
    Buffer* buffer;
};

struct BlitOp
{
    std::optional<uint32_t> srcMip;
    std::optional<uint32_t> dstMip;
};

class CommandBuffer
{
public:
    virtual ~CommandBuffer(){};

    void BindSubmesh(const Submesh& submesh);

    // virtual void BindResource(RefPtr<Gfx::ShaderResource> resource) = 0;
    virtual void BindResource(uint32_t set, Gfx::ShaderResource* resource) = 0;
    virtual void BindVertexBuffer(
        std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
    ) = 0;
    virtual void BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType) = 0;
    virtual void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const Gfx::ShaderConfig& config) = 0;

    virtual void BeginRenderPass(Gfx::RenderPass& renderPass, const std::vector<Gfx::ClearValue>& clearValues) = 0;
    virtual void NextRenderPass() = 0;
    virtual void EndRenderPass() = 0;

    virtual void DrawIndexed(
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
    ) = 0;
    virtual void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp = {}) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;

    virtual void PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings) = 0;
    virtual void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) = 0;
    virtual void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) = 0;
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void DispatchIndir(Buffer* buffer, size_t bufferOffset) = 0;
    virtual void CopyBuffer(
        RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions
    ) = 0;
    void CopyBuffer(
        RefPtr<Gfx::Buffer> bDst,
        RefPtr<Gfx::Buffer> bSrc,
        uint64_t size,
        uint64_t dstOffset = 0,
        uint64_t srcOffset = 0
    )
    {
        BufferCopyRegion r[1]{{srcOffset, dstOffset, size}};
        CopyBuffer(bSrc, bDst, r);
    }
    virtual void CopyImageToBuffer(
        RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions
    ) = 0;
    virtual void CopyBufferToImage(
        RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
    ) = 0;
    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual void Reset(bool releaseResource) = 0;

    virtual void AllocateAttachmentRT(RG::AttachmentIdentifier identifier, const ImageDescription& desc) = 0;
    virtual void BeginRenderPass(RG::RenderPass& renderPass, std::span<ClearValue> clearValues) = 0;
};
} // namespace Gfx
