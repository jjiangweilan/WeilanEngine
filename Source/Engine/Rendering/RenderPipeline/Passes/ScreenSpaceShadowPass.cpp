#include "ScreenSpaceShadowPass.hpp"
#include "Rendering/ShaderLibrary.hpp"

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
