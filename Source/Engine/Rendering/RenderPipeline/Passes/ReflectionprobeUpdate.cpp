#include "ReflectionProbeupdate.hpp"
#include "Core/Component/ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderingUtils.hpp"

namespace Rendering::Passes
{

ReflectionProbeUpdate::ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer)
{
    iblGenerator = ShaderLibrary::GetShader(Shaders::ReflectionProbeIBLGenerator);
    mat.SetShader(iblGenerator);
}

void ReflectionProbeUpdate::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe)
{
    probe.EnsureIBLProbe();

}
} // namespace Rendering::Passes
