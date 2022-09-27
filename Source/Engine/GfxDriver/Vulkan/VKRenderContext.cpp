#include "VKRenderContext.hpp"
#include "VKCommandBuffer.hpp"
#include "Internal/VKSwapChain.hpp"
#include "Exp/VKShaderResource.hpp"
#include "VKShaderProgram.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
    VKRenderContext::VKRenderContext()
    {
    }

    VKRenderContext::~VKRenderContext()
    {
    }

    UniPtr<CommandBuffer> VKRenderContext::CreateCommandBuffer()
    {
        return MakeUnique<VKCommandBuffer>();
    }

    void VKRenderContext::ExecuteCommandBuffer(UniPtr<CommandBuffer>&& commandBuffer)
    {
        pendingCmds.push_back(std::move(commandBuffer));
    }

    void VKRenderContext::Render(VkCommandBuffer cmd)
    {
        if (beginFrameFunc)
        {
            beginFrameFunc(cmd);
            beginFrameFunc = nullptr;
        }

        for(auto& f : pendingCmds)
        {
            static_cast<VKCommandBuffer*>(f.Get())->ExecutePendingCommands(cmd);
        }

        pendingCmds.clear();
    }

    void VKRenderContext::BeginFrame(RefPtr<Gfx::ShaderResource> globalResource_)
    {
        Exp::VKShaderResource* globalResource = (Exp::VKShaderResource*)globalResource_.Get();
        beginFrameFunc = [=](VkCommandBuffer cmd)
        {
            VkDescriptorSet set = globalResource->GetDescriptorSet();
            if (set)
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ((VKShaderProgram*)globalResource->GetShader().Get())->GetVKPipelineLayout(), Scene_Descriptor_Set, 1, &set, 0, VK_NULL_HANDLE);
        };
    }

    void VKRenderContext::EndFrame()
    {

    }
}
