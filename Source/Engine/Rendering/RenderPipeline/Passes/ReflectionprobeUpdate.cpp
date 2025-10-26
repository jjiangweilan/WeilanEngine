#include "ReflectionProbeupdate.hpp"
#include "Core/Component/ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderingUtils.hpp"

namespace Rendering::Passes
{

ReflectionProbeUpdate::ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer)
{
}

void ReflectionProbeUpdate::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe)
{
}
} // namespace Rendering::Passes
