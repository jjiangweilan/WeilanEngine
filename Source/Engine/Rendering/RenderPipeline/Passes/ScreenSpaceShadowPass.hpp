#pragma once
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/Shader.hpp"

namespace Rendering::Passes
{
class ScreenSpaceShadowPass : public RenderPipelinePass
{
public:
    ScreenSpaceShadowPass();

    void OnInit(RenderingData* renderingData) override;

    ObjPtr<Shader> GetShader() const { return shader; }

private:
    ObjPtr<Shader> shader{};
};
} // namespace Rendering::Passes
