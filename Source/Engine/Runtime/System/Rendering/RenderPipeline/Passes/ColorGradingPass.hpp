#pragma once
#include "Driver/GfxDriver/CommandBuffer.hpp"
#include "Driver/GfxDriver/RenderGraph.hpp"
#include "Runtime/System/Rendering/Material.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Runtime/System/Rendering/RenderingData.hpp"

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
