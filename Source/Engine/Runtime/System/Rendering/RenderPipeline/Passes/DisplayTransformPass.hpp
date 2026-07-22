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
class DisplayTransformPass : public RenderPipelinePass
{
public:
    DisplayTransformPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        Gfx::Image* mainColorInput,
        const glm::float2& rtSize,
        const RenderPipelineSetting::PostProcess& settings,
        const RenderingData& renderingData
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return outputId; }

    void OnInit(RenderingData* renderingData) override;

private:
    struct DisplayTransformInput
    {
        glm::uvec4 flags;
        glm::vec4 hsv;
        glm::vec4 exposure;
    };

    Gfx::ImageIdentifier outputId = Gfx::ImageIdentifier("Display Transform");
    Gfx::RenderPass pass = Gfx::RenderPass::SingleColor("Display Transform");
    ObjPtr<Shader> displayTransformShader;
    Material mat;
    ObjPtr<Texture> tonyMcMapfaceLUT;
    PipelineGPUBuffer inputBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("DisplayTransform", PipelineGPUBufferUsage::Uniform);
};
} // namespace Rendering::Passes
