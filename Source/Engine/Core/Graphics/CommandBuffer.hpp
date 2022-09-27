#pragma once

#include "Mesh.hpp"
#include "Shader.hpp"
#include "Material.hpp"
#include "RenderTarget.hpp"
#include "GfxDriver/RenderPass.hpp"
#include "GfxDriver/FrameBuffer.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Utils/Structs.hpp"
#include <memory>

namespace Engine
{
    class CommandBuffer
    {
        public:
            virtual ~CommandBuffer() {};
            virtual void BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass, RefPtr<Gfx::FrameBuffer> frameBuffer, const std::vector<Gfx::ClearValue>& clearValues) = 0;
            virtual void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) = 0;
            virtual void BindResource(RefPtr<Gfx::ShaderResource> resource) = 0;
            virtual void BindVertexBuffer(const std::vector<RefPtr<Gfx::Buffer>>& buffers, const std::vector<uint64_t>& offsets, uint32_t firstBindingIndex) = 0;
            virtual void BindIndexBuffer(Gfx::Buffer* buffer, uint64_t offset) = 0;
            virtual void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const Gfx::ShaderConfig& config) = 0;
            virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) = 0;
            virtual void EndRenderPass() = 0;
            virtual void Render(Mesh& mesh, Material& material) = 0;
            virtual void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) = 0;

    };
}
