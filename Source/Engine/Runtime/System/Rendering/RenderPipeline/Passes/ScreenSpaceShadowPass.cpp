#include "ScreenSpaceShadowPass.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
ScreenSpaceShadowPass::ScreenSpaceShadowPass()
{
    shader = ShaderLibrary::GetShader(Shaders::ScreenSpaceShadow);
}

void ScreenSpaceShadowPass::OnInit(RenderingData* renderingData)
{
}

} // namespace Rendering::Passes
