#include "NodeBuilder.hpp"

namespace RenderGraph
{
// BuildResult NodeBuilder::Blit(const std::vector<BlitDescription>& blits)
// {
//     BuildResult result = {};
//
//     result.execFunc = [blits](Gfx::CommandBuffer& cmd, auto& pass, const auto& res)
//     {
//         for (auto& b : blits)
//         {
//             Gfx::Image* src = (Gfx::Image*)res.at(b.srcHandle)->GetResource();
//             Gfx::Image* dst = (Gfx::Image*)res.at(b.dstHandle)->GetResource();
//             cmd.Blit(src, dst);
//         }
//     };
//
//     for (auto& desc : blits)
//     {
//         result.resourceDescs.push_back({
//             .name = "blit src image",
//             .handle = desc.srcHandle,
//             .type = RenderGraph::ResourceType::Image,
//             .accessFlags = Gfx::AccessMask::Transfer_Read,
//             .stageFlags = Gfx::PipelineStage::Transfer,
//             .imageUsagesFlags = Gfx::ImageUsage::TransferSrc,
//             .imageLayout = Gfx::ImageLayout::Transfer_Src,
//         });
//         result.resourceDescs.push_back({
//             .name = "blit dst image",
//             .handle = desc.dstHandle,
//             .type = RenderGraph::ResourceType::Image,
//             .accessFlags = Gfx::AccessMask::Transfer_Write,
//             .stageFlags = Gfx::PipelineStage::Transfer,
//             .imageUsagesFlags = Gfx::ImageUsage::TransferDst,
//             .imageLayout = Gfx::ImageLayout::Transfer_Dst,
//             .imageCreateInfo = desc.dstCreateInfo,
//         });
//     }
//     return result;
// }
} // namespace RenderGraph
