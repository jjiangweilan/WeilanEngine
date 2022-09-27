#pragma once
#include "Core/Graphics/CommandBuffer.hpp"
#include "Core/Graphics/Material.hpp"
#include "VKRenderTarget.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <unordered_map>
namespace Engine::Gfx
{
namespace Exp
{
    class VKShaderResource;
}
    class VKDevice;
    class VKCommandBuffer : public CommandBuffer
    {
        public:
            VKCommandBuffer();
            VKCommandBuffer(const VKCommandBuffer& other) = delete;
            ~VKCommandBuffer();

            void BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass,
                    RefPtr<Gfx::FrameBuffer> frameBuffer,
                    const std::vector<Gfx::ClearValue>& clearValues) override;

            void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) override;
            // renderpass and framebuffer have to be compatible. https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
            void BindResource(RefPtr<Gfx::ShaderResource> resource) override;
            void BindVertexBuffer(const std::vector<RefPtr<Gfx::Buffer>>& buffers, const std::vector<uint64_t>& offsets, uint32_t firstBindingIndex) override;
            void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const ShaderConfig& config) override;
            void BindIndexBuffer(Gfx::Buffer* buffer, uint64_t offset) override;
            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) override;
            void EndRenderPass() override;
            void Render(Mesh& mesh, Material& material) override;
            void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) override;

            void AppendCustomCommand(std::function<void(VkCommandBuffer)>&& f);
            void ExecutePendingCommands(VkCommandBuffer cmd);

        private:
            struct ExecuteContext
            {
                VkRenderPass currentPass;
                uint32_t subpass;
            } executeContext;

            struct RecordContext
            {
                VkRenderPass currentPass;
                std::unordered_map<VkRenderPass, std::vector<Exp::VKShaderResource*>> bindedResources;
            } recordContext;


            std::vector<std::function<void(VkCommandBuffer, ExecuteContext& context)>> pendingCommands;
    };
}
