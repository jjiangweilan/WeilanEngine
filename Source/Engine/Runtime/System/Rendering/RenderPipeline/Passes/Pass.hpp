#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"

namespace Rendering::Passes
{
class RenderingModule
{
    virtual bool DebugBlit(Gfx::ImageIdentifier& dst) { return false; }
};
} // namespace Rendering::Passes
