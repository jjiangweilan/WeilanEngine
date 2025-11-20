#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/Shader.hpp"

namespace Rendering::Passes
{
class FXAAPass : public RenderPipelinePass
{
public:
    FXAAPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        const glm::float4& sourceSize,
        const Gfx::ImageIdentifier& src,
        const Gfx::ImageIdentifier& dst
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return fxaaId; }

    void OnInit(RenderingData* renderingData) override;

private:
    Gfx::RenderPass pass = Gfx::RenderPass(1, 1);
    Gfx::ImageIdentifier fxaaId = "FXAA";
    ObjPtr<Shader> shader;
    std::unique_ptr<Gfx::ShaderResource> resource;
};
} // namespace Rendering::Passes
