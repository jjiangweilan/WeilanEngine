#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace FrameGraph
{
class ImageNode : public Node
{
    DECLARE_OBJECT();

public:
    ImageNode();
    ImageNode(FGID id);

    Gfx::Image* GetImage();
    void Compile() override;
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override;

private:
    struct
    {
        PropertyHandle attachment;
    } output;

    RenderGraph::RenderNode* imageNode;
    std::unique_ptr<Gfx::Image> image;
    Gfx::RG::ImageIdentifier id;
    Gfx::RG::ImageDescription desc;

    void DefineNode();
    static char _reg;
};
} // namespace FrameGraph
