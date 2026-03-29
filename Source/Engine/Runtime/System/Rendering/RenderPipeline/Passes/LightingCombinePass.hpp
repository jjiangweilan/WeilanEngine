#pragma once
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

namespace Rendering::Passes
{
class LightingCombinePass : public RenderPipelinePass
{
public:
    LightingCombinePass();
    ~LightingCombinePass() = default;

    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier* ssil,
        const Gfx::ImageIdentifier* rtgi,
        const Gfx::ImageIdentifier& albedoTex,
        const Gfx::ImageIdentifier& colorTex,
        RenderingData& renderingData
    );

private:
    Shader* computeShader;
    Material mat;
};
} // namespace Rendering::Passes
