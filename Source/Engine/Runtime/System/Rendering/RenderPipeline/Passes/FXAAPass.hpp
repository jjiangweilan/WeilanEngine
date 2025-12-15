#pragma once
#include "Driver/GfxDriver/CommandBuffer.hpp"
#include "Driver/GfxDriver/RenderGraph.hpp"
#include "Driver/GfxDriver/ShaderResource.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Runtime/System/Rendering/RenderingData.hpp"
#include "Runtime/System/Rendering/Shader.hpp"

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
