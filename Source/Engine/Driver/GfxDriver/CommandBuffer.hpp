#pragma once

#include "Buffer.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include "FrameBuffer.hpp"
#include "GfxEnums.hpp"
#include "Image.hpp"
#include "RenderGraph.hpp"
#include "RenderPass.hpp"
#include "ShaderConfig.hpp"
#include "ShaderResource.hpp"
#include "VertexBufferBinding.hpp"
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
    uint64_t bufferOffset;
    Gfx::ImageSubresourceLayers layers;
    Offset3D offset;
    Extent3D extend;
};

struct Viewport
{
    float x = 0;
    float y = 0;
    float width = 1;
    float height = 1;
    float minDepth = 0;
    float maxDepth = 1;
};

struct RenderAttachment
{
    ImageIdentifier image;
    Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
    Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::Store;
};

struct DescriptorBinding
{
    DescriptorBinding() = default;
    DescriptorBinding(int dstBinding, Image* image);
    DescriptorBinding(int dstBinding, ImageView* imageView);
    DescriptorBinding(int dstBinding, Buffer* buffer);
    int dstBinding;
    int dstArrayElement;
    int descriptorCount;
    ImageView* imageView;
    Buffer* buffer;
};

struct DynamicBinding
{
    DynamicBinding(std::string_view name, Gfx::Buffer& buffer) : name(name), buffer(&buffer), imageIdentifier()
    {}

    DynamicBinding(std::string_view name, Gfx::Image& image) : name(name), buffer(nullptr), imageIdentifier(image)
    {}

    DynamicBinding(std::string_view name, Gfx::ImageView& imageView) : name(name), buffer(nullptr), imageIdentifier(imageView)
    {}

    DynamicBinding(std::string_view name, const ImageIdentifier& id) : name(name), buffer(nullptr), imageIdentifier(id)
    {}

    std::string name;
    Gfx::Buffer* buffer;
    ImageIdentifier imageIdentifier;
};

struct BlitOp
{
    std::optional<uint32_t> srcMip;
    std::optional<uint32_t> dstMip;
};

struct AsyncReadbackHandle
{
public:
    virtual uint8_t* GetData() { return nullptr; }
    virtual bool IsComplete() { return false; }
};

class CommandBuffer
{
public:
    virtual ~CommandBuffer() {};

    virtual void BeginLabel(std::string_view label, const glm::float4& color) = 0;
    virtual void BeginLabel(std::string_view label, float color[4]) = 0;
    virtual void EndLabel() = 0;
    virtual void InsertLabel(std::string_view label, float color[4]) = 0;
    virtual void InsertLabel(std::string_view label, const glm::float4& color) = 0;

    virtual void BindResource(uint32_t set, Gfx::ShaderResource* resource) = 0;
    virtual void BindResource(uint32_t set, const std::vector<DynamicBinding>& bindings) = 0;
    virtual void BindVertexBuffer(
        std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
    ) = 0;
    virtual void BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType) = 0;
    virtual void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const PipelineConfig& config) = 0;

    virtual void BeginRenderPass(std::span<const RenderAttachment> images, std::span<Gfx::ClearValue> clearValues) = 0;
    virtual void BeginRenderPass(Gfx::RenderPass_Deprecated& renderPass, std::span<Gfx::ClearValue> clearValues) = 0;
    virtual void NextRenderPass() = 0;
    virtual void EndRenderPass() = 0;

    virtual void DrawIndexed(
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
    ) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
    virtual void DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp = {}) = 0;
    virtual void GraphicsBlit(const Gfx::ImageIdentifier& from, const Gfx::ImageIdentifier& to) = 0;

    virtual void PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings) = 0;
    virtual void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) = 0;
    virtual void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) = 0;
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetLineWidth(float lineWidth) = 0;
    virtual void SetDepthBias(float constantFactor, float clamp, float slopeFactor) = 0;
    virtual void SetDepthBiasEnable(bool enable) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void DispatchIndirect(Buffer* buffer, size_t bufferOffset) = 0;
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
    virtual void ClearColorImage(Image* image, const ClearColor& color) = 0;

    virtual void SetTexture(
        ShaderBindingHandle name,
        int index,
        ImageIdentifier id,
        std::optional<ImageViewOption> imageViewOption = std::nullopt
    ) = 0;
    virtual void SetTexture(
        ShaderBindingHandle name,
        int index,
        Gfx::Image& image,
        std::optional<ImageViewOption> imageViewOption = std::nullopt
    ) = 0;
    virtual void SetBuffer(ShaderBindingHandle name, int index, Gfx::Buffer& buffer) = 0;

    virtual std::shared_ptr<AsyncReadbackHandle> AsyncReadback(Gfx::Buffer& buffer, size_t size, size_t offset = 0) = 0;

    virtual void AllocateAttachment(const ImageIdentifier& id, RenderImageDescriptor& desc) = 0;
    virtual void BeginRenderPass(RenderPass& renderPass, std::span<ClearValue> clearValues) = 0;

    virtual void Blit(ImageIdentifier src, ImageIdentifier dst, BlitOp blitOp = {}) = 0;

    void UpdateViewportAndScissor(uint32_t width, uint32_t height)
    {
        Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};
        SetScissor(0, 1, &scissor);
        Gfx::Viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        SetViewport(viewport);
    }

    // note: currently to correctly setup global binding, this function should be called before BindShaderProgram
    void SetTexture(
        ShaderBindingHandle name, ImageIdentifier id, std::optional<ImageViewOption> imageViewOption = std::nullopt
    )
    {
        SetTexture(name, 0, id, imageViewOption);
    }

    void SetTexture(
        ShaderBindingHandle name, Gfx::Image& image, std::optional<ImageViewOption> imageViewOption = std::nullopt
    )
    {
        SetTexture(name, 0, image, imageViewOption);
    }

    void SetBuffer(ShaderBindingHandle name, Gfx::Buffer& buffer) { SetBuffer(name, 0, buffer); }

    void SetTexture(
        std::string_view name, Gfx::Image& image, std::optional<ImageViewOption> imageViewOption = std::nullopt
    )
    {
        SetTexture(ShaderBindingHandle(name), 0, image, imageViewOption);
    }

    void SetTexture(
        std::string_view name, ImageIdentifier id, std::optional<ImageViewOption> imageViewOption = std::nullopt
    )
    {
        SetTexture(ShaderBindingHandle(name), 0, id, imageViewOption);
    }

    void SetBuffer(std::string_view name, Gfx::Buffer& buffer) { SetBuffer(ShaderBindingHandle(name), 0, buffer); }
};
} // namespace Gfx
