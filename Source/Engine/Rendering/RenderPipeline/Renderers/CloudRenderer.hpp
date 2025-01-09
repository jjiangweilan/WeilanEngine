#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Material.hpp"

namespace Rendering
{
class CloudRenderer
{
public:
    void Init();
    void Render(Gfx::CommandBuffer& cmd);
private:

    Material cloudMaterial;
};
} // namespace Rendering
