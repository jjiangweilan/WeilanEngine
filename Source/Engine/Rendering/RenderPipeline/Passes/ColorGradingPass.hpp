#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Rendering/RenderingData.hpp"

namespace Rendering::Passes
{
class ColorGradingPass : public RenderPipelinePass
{
public:
    ColorGradingPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        Gfx::Image* mainColorInput,
        const glm::float2& rtSize
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return colorGradingId; }

    void OnInit(RenderingData* renderingData) override;

private:
    Gfx::ImageIdentifier colorGradingId = Gfx::ImageIdentifier("Color Grading");
    Gfx::RenderPass pass = Gfx::RenderPass::SingleColor("Color Grading");
    ObjPtr<Shader> colorGradingShader;
    Material mat;
};
} // namespace Rendering::Passes
