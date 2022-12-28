#include "ImageNode.hpp"

namespace Engine::RGraph
{
ImageNode::ImageNode() { outputPort = AddPort("Output", Port::Type::Output, typeid(Gfx::Image)); }

bool ImageNode::Preprocess(ResourceStateTrack& stateTrack)
{
    auto& state = stateTrack.GetState(outputPort->GetResource());
    if (type == Type::Color) state.usages |= Gfx::ImageUsage::ColorAttachment;
    else if (type == Type::Depth) state.usages |= Gfx::ImageUsage::DepthStencilAttachment;

    return true;
}

bool ImageNode::Compile(ResourceStateTrack& track)
{
    Gfx::ImageDescription description{
        .data = nullptr,
        .width = width,
        .height = height,
        .format = format,
        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
        .mipLevels = 1,
    };

    image = GetGfxDriver()->CreateImage(description, track.GetState(outputPort->GetResource()).usages);

    return true;
}
} // namespace Engine::RGraph
