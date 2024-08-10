#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace Rendering::FrameGraph
{
class RadianceCascadeNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(RadianceCascadeNode)
    {
        input.colorMap = AddInputProperty("colorMap", PropertyType::Attachment);
        input.normalMap = AddInputProperty("normalMap", PropertyType::Attachment);
        input.depthMap = AddInputProperty("depthMap", PropertyType::Attachment);
    }

    const Gfx::RG::ImageIdentifier& GetImage();
    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override {}

private:
    struct
    {
        PropertyHandle colorMap;
        PropertyHandle normalMap;
        PropertyHandle depthMap;
    } input;

    struct
    {

    } output;
};

DEFINE_FRAME_GRAPH_NODE(RadianceCascadeNode, "8FA09CAB-F2FA-4D78-B6C1-624D0B1CD622");
} // namespace Rendering::FrameGraph
