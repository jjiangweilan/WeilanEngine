#include "RayTracingTestPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

namespace Rendering::Passes
{
RayTracingTestPass::RayTracingTestPass()
{
    shader = ShaderLibrary::GetShader(Shaders::RayTracingTest);
}

void RayTracingTestPass::Execute(
    Gfx::CommandBuffer& cmd,
    Gfx::ImageIdentifier& depthTex,
    Gfx::RayTracingSceneHandle tlas,
    Gfx::RayTracingContext* rtContext,
    Gfx::ShaderResource* perSceneResource,
    glm::float2 screenSize
)
{
    if (tlas == -1 || rtContext == nullptr)
        return;

    uint32_t width = static_cast<uint32_t>(screenSize.x) / 2;
    uint32_t height = static_cast<uint32_t>(screenSize.y) / 2;

    Gfx::RenderImageDescriptor desc(width, height, Gfx::GfxFormat::R8_UNorm);
    cmd.AllocateAttachment(outputId, desc);

    Gfx::RenderAttachment color[] = {{outputId, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}};
    Gfx::ClearValue clears[] = {{1, 1, 1, 1}};

    cmd.BeginLabel("Ray Tracing Test Pass", {0.4123, 0.623, 0.323, 1.0f});
    cmd.BeginRenderPass(color, clears);

    std::vector<Gfx::DynamicBinding> dynamicBindings{
        Gfx::DynamicBinding("depthTex", depthTex),
        Gfx::DynamicBinding("sceneBVH", rtContext, tlas)
    };

    auto shaderProgram = shader->GetShaderProgram();
    cmd.BindResource(0, perSceneResource);
    cmd.BindResource(1, dynamicBindings);
    cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultPipelineConfig());
    cmd.Draw(6, 1, 0, 0);

    cmd.EndRenderPass();
    cmd.EndLabel();
}
} // namespace Rendering::Passes
