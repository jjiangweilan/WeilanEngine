#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBuffer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"

class Texture;

namespace Rendering::Passes
{
class ColorGradingPass : public RenderPipelinePass
{
public:
    ColorGradingPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        Gfx::Image* mainColorInput,
        const glm::float2& rtSize,
        const RenderPipelineSetting::PostProcess& settings,
        const RenderingData& renderingData
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return colorGradingId; }

    void OnInit(RenderingData* renderingData) override;

private:
    struct ColorGradingInput
    {
        glm::uvec4 flags;
        glm::vec4 hsv;
    };

    Gfx::ImageIdentifier colorGradingId = Gfx::ImageIdentifier("Color Grading");
    Gfx::RenderPass pass = Gfx::RenderPass::SingleColor("Color Grading");
    ObjPtr<Shader> colorGradingShader;
    Material mat;
    ObjPtr<Texture> tonyMcMapfaceLUT;
    PipelineGPUBuffer colorGradingInputBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("ColorGrading", PipelineGPUBufferUsage::Uniform);
};
} // namespace Rendering::Passes
