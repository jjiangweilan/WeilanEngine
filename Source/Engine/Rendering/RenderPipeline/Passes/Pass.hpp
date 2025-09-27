#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"

namespace Rendering::Passes
{
class RenderingModule
{
    virtual bool DebugBlit(Gfx::ImageIdentifier& dst) { return false; }
};
} // namespace Rendering::Passes
