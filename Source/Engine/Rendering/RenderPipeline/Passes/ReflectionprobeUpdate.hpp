#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/RenderingData.hpp"

class ReflectionProbe;

namespace Rendering::Passes
{
class ReflectionProbeUpdate
{

public:
    ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe);
};
} // namespace Rendering::Passes
