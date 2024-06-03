#pragma once
#include "GfxDriver/RenderGraph.hpp"

namespace Rendering
{
class RenderingUtils
{
public:
    static void DynamicScaleImageDescription(Gfx::RG::ImageDescription& desc, const glm::vec2& scale);
};

} // namespace Rendering
