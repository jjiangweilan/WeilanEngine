#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"

namespace Rendering
{
class RenderingUtils
{
public:
    static void DynamicScaleImageDescription(Gfx::RG::ImageDescription& desc, const glm::vec2& scale);

    // dispatch all registered graphics command in Graphics.DrawXXX API
    // this should be called inside a RenderPass
    static void DrawGraphics(Gfx::CommandBuffer& cmd);
};

} // namespace Rendering
