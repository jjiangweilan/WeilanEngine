#pragma once
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

namespace Rendering::Passes
{
class SSIL : public RenderPipelinePass
{
public:
    SSIL();
    ~SSIL() = default;

    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& colorTex,
        const Gfx::ImageIdentifier& depthTex,
        const Gfx::ImageIdentifier& albedoTex,
        const Gfx::ImageIdentifier& normalTex,
        const Gfx::ImageIdentifier& targetColor,
        RenderPipelineSetting* setting,
        RenderingData& renderingData
    );

    Gfx::ImageIdentifier& GetOutputId() { return ssil; }

    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    Shader* ssilShader;
    Material mat;
    Gfx::ImageIdentifier ssil = "SSIL_Output";

    Gfx::PipelineConfig combineConfig;

    bool debugSSIL = false;
};
} // namespace Rendering::Passes
