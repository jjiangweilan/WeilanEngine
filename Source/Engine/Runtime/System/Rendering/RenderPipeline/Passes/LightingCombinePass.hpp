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
        const Gfx::ImageIdentifier* rtgiOutput,
        const Gfx::ImageIdentifier* rtgiSH0,
        const Gfx::ImageIdentifier* rtgiSH1,
        const Gfx::ImageIdentifier* rtgiSH2,
        const Gfx::ImageIdentifier& albedoTex,
        const Gfx::ImageIdentifier& normalTex,
        const Gfx::ImageIdentifier& colorTex,
        const Gfx::ImageIdentifier& hierarchyDepth,
        RenderingData& renderingData
    );

private:
    Shader* computeShader;
    Material mat;
};
} // namespace Rendering::Passes
