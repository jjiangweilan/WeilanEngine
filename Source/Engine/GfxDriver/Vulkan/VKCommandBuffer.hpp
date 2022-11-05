#pragma once
#include "../CommandBuffer.hpp"
#include "VKRenderTarget.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <unordered_map>
namespace Engine::Gfx
{
    class VKShaderResource;
    class VKDevice;
    class VKCommandBuffer : public CommandBuffer
    {
        public:
            VKCommandBuffer();
            VKCommandBuffer(const VKCommandBuffer& other) = delete;
            ~VKCommandBuffer();

            void BeginRenderPass(RefPtr<Gfx::RenderPass> renderPass,
                    const std::vector<Gfx::ClearValue>& clearValues) override;
            void EndRenderPass() override;

            void Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to) override;
            // renderpass and framebuffer have to be compatible. https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
            void BindResource(RefPtr<Gfx::ShaderResource> resource) override;
            void BindVertexBuffer(const std::vector<RefPtr<Gfx::GfxBuffer>>& buffers, const std::vector<uint64_t>& offsets, uint32_t firstBindingIndex) override;
            void BindShaderProgram(RefPtr<Gfx::ShaderProgram> program, const ShaderConfig& config) override;
            void BindIndexBuffer(RefPtr<Gfx::GfxBuffer> buffer, uint64_t offset, IndexBufferType indexBufferType) override;
            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) override;
            void SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data) override;
            void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect) override;
            void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
            void NextRenderPass() override;

            void AppendCustomCommand(std::function<void(VkCommandBuffer)>&& f);
            void RecordToVulkanCmdBuf(VkCommandBuffer cmd);

        private:
            struct ExecuteContext
            {
                VkRenderPass currentPass = VK_NULL_HANDLE;
                uint32_t subpass = -1;
            } executeContext;

            struct RenderPassResources
            {
                std::vector<VKShaderResource*> bindedResources;
                std::vector<VKBuffer*> vertexBuffers;
                std::vector<VKBuffer*> indexBuffers;
            };

            struct RecordContext
            {
                VkRenderPass currentPass = VK_NULL_HANDLE;
                std::unordered_map<VkRenderPass, RenderPassResources> renderPassResources;

                // these data will be moved to renderPassResources in EndRenderPass()
                std::vector<VKShaderResource*> bindedResources;
                std::vector<VKBuffer*> vertexBuffers;
                std::vector<VKBuffer*> indexBuffers;
            } recordContext;


            std::vector<std::function<void(VkCommandBuffer, ExecuteContext& context, RecordContext& recordContext)>> pendingCommands;
    };
}
