#pragma once
#include "Engine/Library/CommandStream.hpp"
#include "RenderCoreData.hpp"

struct AllocateTempImageData
{
    RenderImageDescriptor desc;
    ImageIdentifier id;
};

class RenderCommandBufferCommandStreamContext : public CommandStreamContext
{
};

class RenderResourceAllocator
{
public:
    void AllocateTempImage(const RenderImageDescriptor& desc, ImageIdentifier& id);
};

class RenderComandBuffer
{
public:
    void AllocateTempImage(const RenderImageDescriptor& desc, ImageIdentifier& id);
    void BeginLabel(std::string_view label);
    void EndLabel();
    void SetRenderPass(std::span<const RenderAttachment> images);
    void SetClearValues(std::span<Gfx::ClearValue> clearValues);
    void BindResource(uint32_t set, Gfx::ShaderResource* resource);
    void BindResource(uint32_t set, const std::vector<DynamicBinding>& bindings);
    void SetPushConstant(void* data);
    void DrawMesh(const MeshHandle& meshHandle, Gfx::ShaderProgram* shaderProgram, const Gfx::PipelineConfig& pipelineConfig);
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void Blit(ImageIdentifier src, ImageIdentifier dst, BlitOp blitOp = {});

    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect);
    void SetViewport(const Viewport& viewport);
    void SetLineWidth(float lineWidth);
    void SetDepthBias(float constantFactor, float clamp, float slopeFactor);
    void SetDepthBiasEnable(bool enable);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndirect(Gfx::Buffer* buffer, size_t bufferOffset);

private:
    CommandStream cm;
    RenderResourceAllocator* resourceAllocator;
};

struct BeginLabelCmd
{
    const char* str;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        BeginLabelCmd* cmd = (BeginLabelCmd*)ptr;
    }
};
