#include "NodeBuilder.hpp"

namespace Engine::RenderGraph
{
BuildResult NodeBuilder::Blit(const std::vector<BlitDescription>& blits)
{
    BuildResult result = {};

    result.execFunc = [blits](Gfx::CommandBuffer& cmd, auto& pass, auto& res)
    {
        int handle = 0;
        for (auto& b : blits)
        {
            Gfx::Image* src = (Gfx::Image*)b.srcNode.GetPass()->GetResourceRef(b.srcHandle)->GetResource();
            Gfx::Image* dst = (Gfx::Image*)res.at(handle);
            cmd.Blit(src, dst);
        }
        handle += 1;
    };

    for (auto& desc : blits)
    {
        result.resourceDescs.push_back({
            .name = "SceneColorEditorCopy",
            .handle = desc.dstHandle,
            .type = RenderGraph::ResourceType::Image,
            .accessFlags = Gfx::AccessMask::Transfer_Write,
            .stageFlags = Gfx::PipelineStage::Transfer,
            .imageUsagesFlags = Gfx::ImageUsage::TransferDst,
            .imageLayout = Gfx::ImageLayout::Transfer_Dst,
            .imageCreateInfo = desc.dstCreateInfo,
        });
    }
    return result;
}
} // namespace Engine::RenderGraph
