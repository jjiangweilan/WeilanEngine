#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace Engine::FrameGraph
{
class ImageNode : public Node
{
public:
    ImageNode(FGID id) : Node("Image", id), output(true)
    {
        AddOutputProperty("Image", PropertyType::Image, &output);

        configs = {
            Configurable::C<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f}),
            Configurable::C<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm),
            Configurable::C<ConfigurableType::Int>("mip level", int{1}),
        };
    }

    void Build(BuildResources& resources){};

private:
    ImageProperty output;

    static char _reg;
};

char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
} // namespace Engine::FrameGraph
