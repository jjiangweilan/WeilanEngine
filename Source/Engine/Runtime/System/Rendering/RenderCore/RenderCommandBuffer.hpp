#pragma once
#include "Engine/Library/CommandStream.hpp"
#include "RenderCoreData.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

class CommandStreamProcessor;
class RenderResourceAllocator;

class RenderCommandBuffer
{
public:
    RenderCommandBuffer(CommandStreamProcessor* context, RenderResourceAllocator* resourceAllocator);

    void AllocateTempImage(const Gfx::RenderImageDescriptor& desc, Gfx::ImageIdentifier& id);

    void BeginLabel(std::string_view label, const float4& color);
    void EndLabel();
    void SetRenderPass(std::span<const Gfx::RenderAttachment> images);
    void SetClearValues(std::span<Gfx::ClearValue> clearValues);
    void BindResource(uint32_t set, Gfx::ShaderResource* resource);
    void BindResource(uint32_t set, const std::vector<Gfx::DynamicBinding>& bindings);
    void SetPushConstant(void* data);
    void DrawMesh(const MeshHandle& meshHandle, Gfx::ShaderProgram* shaderProgram, const Gfx::PipelineConfig& pipelineConfig);
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void Blit(Gfx::ImageIdentifier src, Gfx::ImageIdentifier dst, Gfx::BlitOp blitOp = {});
    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect);
    void SetViewport(const Gfx::Viewport& viewport);
    void SetLineWidth(float lineWidth);
    void SetDepthBias(float constantFactor, float clamp, float slopeFactor);
    void SetDepthBiasEnable(bool enable);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndirect(Gfx::Buffer* buffer, size_t bufferOffset);

private:
    CommandStream cm;
    RenderResourceAllocator* resourceAllocator;
};
