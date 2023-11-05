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
    ImageNode() {}
    ImageNode(FGID id) : Node("Image", id)
    {
        AddOutputProperty("Image", PropertyType::Image, nullptr);

        configs = {
            Configurable::C<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f}),
            Configurable::C<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm),
            Configurable::C<ConfigurableType::Int>("mip level", int{1}),
        };
    }

    void Preprocess(RenderGraph::Graph& graph) override{};
    void Build(RenderGraph::Graph& graph, BuildResources& resources) override{};

private:
    static char _reg;
};

char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
DEFINE_OBJECT(ImageNode, "FE0667ED-89FA-4986-842B-158654543C18");
} // namespace Engine::FrameGraph
