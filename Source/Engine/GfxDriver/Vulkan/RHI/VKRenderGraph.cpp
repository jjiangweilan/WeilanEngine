#include "VKRenderGraph.hpp"

namespace Gfx::VK::RenderGraph
{

void BuildDependencyGraph() {}

void Graph::RegisterRenderPass(int& visitIndex)
{
    assert(activeSchedulingCmds[visitIndex].type == VKCmdType::BeginRenderPass);
    auto& cmd = activeSchedulingCmds[visitIndex].beginRenderPass;

    RenderingOperation op{RenderingOperationType::RenderPass};
    renderingOperations.push_back(op);
    size_t renderingOpIndex = renderingOperations.size() - 1;

    // 18/01/2024: I haven't actually use subpass now, so I treat the first subpass as SetAttachment and AddSubpass(0)
    if (!cmd.renderPass->GetSubpesses().empty())
    {
        auto& s = cmd.renderPass->GetSubpesses()[0];
        for (auto& c : s.colors)
        {
            void* image = &c.imageView->GetImage();
            auto iter = resourceUsageTracks.find(image);

            if (iter != resourceUsageTracks.end())
            {
                iter->second.usages.push_back(renderingOpIndex);
            }
            else
            {
                ResourceUsageTrack track;
                track.type = ResourceType::Image;
                track.res = c.imageView;
                track.usages.push_back(renderingOpIndex);
            }
        }

        if (s.depth.has_value())
        {
            void* image = &s.depth->imageView->GetImage();
            auto iter = resourceUsageTracks.find(image);

            if (iter != resourceUsageTracks.end())
            {
                iter->second.usages.push_back(renderingOpIndex);
            }
            else
            {
                ResourceUsageTrack track;
                track.type = ResourceType::Image;
                track.res = s.depth->imageView;
                track.usages.push_back(renderingOpIndex);
            }
        }
    }

    // proceed to End Render Pass, collect GPU resources that are going to be written alone the way
    for (;;)
    {
        visitIndex += 1;
        auto& cmd = activeSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::EndRenderPass)
            break;
        else if (cmd.type == VKCmdType::BindResource)
        {
            state.bindedResources[cmd.bindResource.set] = visitIndex;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            state.bindedProgram = visitIndex;
        }
        else if (cmd.type == VKCmdType::Draw || cmd.type == VKCmdType::DrawIndexed)
            FlushBindResourceTrack();
    }
}

void Graph::FlushBindResourceTrack() {}

void Graph::Schedule(VKCommandBuffer2& cmd)
{
    activeSchedulingCmds = cmd.GetCmds();

    // register rendering operation
    for (int visitIndex = 0; visitIndex < activeSchedulingCmds.size(); visitIndex++)
    {
        auto& cmd = activeSchedulingCmds[visitIndex];
        switch (cmd.type)
        {
            case VKCmdType::BeginRenderPass: RegisterRenderPass(visitIndex); break;
            case VKCmdType::EndRenderPass: break;
        }
    }
}
} // namespace Gfx::VK::RenderGraph
