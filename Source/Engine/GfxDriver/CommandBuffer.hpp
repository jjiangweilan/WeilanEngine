#pragma once

#include "ShaderResource.hpp"
#include "RenderPass.hpp"
#include "FrameBuffer.hpp"
#include "Image.hpp"
#include "ShaderConfig.hpp"
#include "GfxBuffer.hpp"
#include "Utils/Structs.hpp"
#include <memory>

namespace Engine
{
    enum class IndexBufferType
    {
        UInt16, UInt32  
    };

    class CommandBuffer
    {
        public:
            virtual ~CommandBuffer() {};
            virtual void BindResource(RefPtr<Gfx::ShaderResource> resource) = 0;
            virtual void BindVertexBuffer(const std::vector<RefPtr<Gfx::GfxBuffer>>& buffers, const std::vector<uint64_t>& offsets, uint32_t firstBindingIndex) = 0;
            virtual void BindIndexBuffer(RefPtr<Gfx::GfxBuffer> buffer, uint64_t offset, IndexBufferType indexBufferType) = 0;
            virtual void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const Gfx::ShaderConfig& config) = 0;

            virtual void BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass, RefPtr<Gfx::FrameBuffer> frameBuffer, const std::vector<Gfx::ClearValue>& clearValues) = 0;
            virtual void EndRenderPass() = 0;

            virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) = 0;
            virtual void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) = 0;

            virtual void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) = 0;
            virtual void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) = 0;

    };
}
