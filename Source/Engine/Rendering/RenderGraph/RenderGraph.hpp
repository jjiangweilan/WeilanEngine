#pragma once
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/CommandBuffer.hpp"

namespace Engine::Rendering
{

class RenderGraph
{
public:
    using ResourceHandle = size_t;
    ResourceHandle AllocateRT(std::string_view name, uint32_t width, uint32_t height, Gfx::ImageFormat format);
    ResourceHandle AllocateBuffer();

    // override next RenderPass viewport and scissor settings
    void SetViewport();
    void SetScissor();

    void BeginRenderPass();
    void NextRenderPass();
    void EndRenderPass();
    void DrawIndexed();
    void Draw();

    void Blit();
    void PushDescriptor();

    void BindShaderProgram();
    void BindResource();
    void BindVertexBuffer();
    void BindIndexBuffer();

    void SetPushConstant();

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndir(Gfx::Buffer& buffer, size_t bufferOffset);

    void UpdateBuffer(Gfx::Buffer& dst, size_t offset, uint8_t* data, size_t size);
    void CopyBuffer(Gfx::Buffer& dst, Gfx::Buffer& src, Gfx::BufferCopyRegion copyRegions);

    void Compile();

private:
    void Execute();

    friend class Server;
};
} // namespace Engine::Rendering
