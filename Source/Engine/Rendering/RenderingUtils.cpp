#include "RenderingUtils.hpp"
#include "Graphics.hpp"

namespace Rendering
{
void RenderingUtils::DynamicScaleImageDescription(Gfx::RG::ImageDescription& desc, const glm::vec2& scale)
{
    const auto& baseSize = glm::vec2{desc.GetWidth(), desc.GetHeight()};
    if (scale.x == 0)
    {
        desc.SetWidth(baseSize.x);
    }
    else if (scale.x < 0)
    {
        desc.SetWidth(baseSize.x * -scale.x);
    }
    else
        desc.SetWidth(scale.x);
    if (scale.y == 0)
    {
        desc.SetHeight(baseSize.y);
    }
    else if (scale.y < 0)
    {
        desc.SetHeight(baseSize.y * -scale.y);
    }
    else
        desc.SetHeight(scale.y);
}

void RenderingUtils::DrawGraphics(Gfx::CommandBuffer& cmd)
{
    Graphics::GetSingleton().DispatchDraws(cmd);
}
} // namespace Rendering
