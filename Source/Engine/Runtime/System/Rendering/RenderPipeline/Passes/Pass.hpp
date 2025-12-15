#pragma once
#include "Driver/GfxDriver/GfxDriver.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"

namespace Rendering::Passes
{
class RenderingModule
{
    virtual bool DebugBlit(Gfx::ImageIdentifier& dst) { return false; }
};
} // namespace Rendering::Passes
