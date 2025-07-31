#pragma once
#include "GfxDriver/GfxDriver.hpp"

namespace Rendering::Passes
{
class RenderingModule
{
    virtual bool DebugBlit(Gfx::RG::ImageIdentifier& dst) { return false; }
};
} // namespace Rendering::Passes
