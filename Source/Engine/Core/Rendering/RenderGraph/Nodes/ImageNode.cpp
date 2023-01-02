#include "ImageNode.hpp"

namespace Engine::RGraph
{
ImageNode::ImageNode() : imageRes(ResourceRef(typeid(Gfx::Image)))
{
    outputPort = AddPort("Output", Port::Type::Output, typeid(Gfx::Image));
    outputPort->SetResource(&imageRes);
}

bool ImageNode::Preprocess(ResourceStateTrack& stateTrack) { return true; }

bool ImageNode::Compile(ResourceStateTrack& stateTrack)
{
    if (!externalImage)
    {
        Gfx::ImageDescription description{
            .data = nullptr,
            .width = width,
            .height = height,
            .format = format,
            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
            .mipLevels = 1,
        };

        auto usageFlags = stateTrack.GetState(outputPort->GetResource()).usages;
        assert(usageFlags != 0);
        image = GetGfxDriver()->CreateImage(description, usageFlags);
        imageRes.SetVal(image.Get());
    }
    else imageRes.SetVal(externalImage);

    outputPort->SetResource(&imageRes);
    return true;
}

bool ImageNode::Execute(CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack)
{
    if (externalImage != nullptr)
    {
        stateTrack.GetState(&imageRes) = initialState;
    }

    return true;
}
} // namespace Engine::RGraph
