#pragma once
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Runtime/System/Rendering/RenderingData.hpp"
#include "Runtime/System/Rendering/Shader.hpp"

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
