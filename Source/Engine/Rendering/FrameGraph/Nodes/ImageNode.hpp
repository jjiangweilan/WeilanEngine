#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace Engine::FrameGraph
{
class ImageNode : public Node
{
    DECLARE_OBJECT();

public:
    ImageNode();
    ImageNode(FGID id);

    Gfx::Image* GetImage();
    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override;

private:
    RenderGraph::RenderNode* imageNode;

    void DefineNode();
    static char _reg;
};
} // namespace Engine::FrameGraph
