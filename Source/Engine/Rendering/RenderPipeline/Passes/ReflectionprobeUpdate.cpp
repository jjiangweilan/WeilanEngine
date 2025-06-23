#include "ReflectionProbeUpdate.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderingData.hpp"

namespace Rendering::Passes
{

void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    auto& renderingScene = renderingData.scene->GetRenderingScene();
    auto renderers = renderingScene.QueryRendererInFrustum(renderingData.cameraFrustum);
}
} // namespace Rendering::Passes
