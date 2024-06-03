#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace Rendering::FrameGraph
{
class ImageNode : public Node
{
    DECLARE_OBJECT();

public:
    ImageNode();
    ImageNode(FGID id);

    const Gfx::RG::ImageIdentifier& GetImage();
    void Compile() override;
    void Execute(Gfx::CommandBuffer& cmd, FrameData& renderingData) override;

    glm::ivec2 GetRenderingImageSize()
    {
        return {desc.GetWidth(), desc.GetHeight()};
    }

private:
    struct
    {
        PropertyHandle attachment;
    } output;

    Gfx::RG::ImageIdentifier id;
    Gfx::RG::ImageDescription desc;

    glm::vec2* size;
    Gfx::ImageFormat* format;

    void DefineNode();
    static char _reg;
};
} // namespace Rendering::FrameGraph
