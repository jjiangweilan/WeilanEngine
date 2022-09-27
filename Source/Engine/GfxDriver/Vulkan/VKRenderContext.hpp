#include "Core/Graphics/RenderContext.hpp"

#include <vulkan/vulkan.h>
#include <functional>
namespace Engine::Gfx
{
    class VKSwapChain;
    class VKRenderContext : public RenderContext
    {
        public:
            VKRenderContext();
            ~VKRenderContext() override;

            // Gfx api
            UniPtr<CommandBuffer> CreateCommandBuffer() override;
            void ExecuteCommandBuffer(UniPtr<CommandBuffer>&& commandBuffer) override;
            void BeginFrame(RefPtr<Gfx::ShaderResource> globalResource) override;
            void EndFrame() override;

            void Render(VkCommandBuffer cmd);
        private:

            std::vector<UniPtr<CommandBuffer>> pendingCmds;
            std::function<void(VkCommandBuffer cmd)> beginFrameFunc;
            VKSwapChain* swapChain;
    };
}
