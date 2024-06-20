#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/DrawList.hpp"
namespace Rendering::LFP
{
struct ProbeFace
{
    std::unique_ptr<Gfx::RenderPass> gbufferPass;
    std::unique_ptr<Gfx::ImageView> albedoView;
    std::unique_ptr<Gfx::ImageView> normalView;
    std::unique_ptr<Gfx::ImageView> depthView;

    void Init(Gfx::Image* albedoCubemap, Gfx::Image* normalCubemap, Gfx::Image* depthCubeMap, uint32_t face)
    {
        gbufferPass = GetGfxDriver()->CreateRenderPass();
        albedoView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *albedoCubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, face, 1}});

        normalView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *normalCubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, face, 1}});

        depthView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *depthCubeMap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Depth, 0, 1, face, 1}});

        Gfx::Attachment albedoAttachment{
            albedoView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear};
        Gfx::Attachment normalAttachment{
            normalView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear};
        Gfx::Attachment depthAttachment{
            depthView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear};

        gbufferPass->AddSubpass({albedoAttachment, normalAttachment}, depthAttachment);
    };
};
class ProbeBaker
{
    ProbeFace faces[6];
    std::unique_ptr<Gfx::Image> albedoCubemap;
    std::unique_ptr<Gfx::Image> normalCubemap;
    std::unique_ptr<Gfx::Image> depthCubeMap;

    const uint32_t rtWidth = 128;
    const uint32_t rtHeight = 128;

public:
    ProbeBaker()
    {
        for (int face = 0; face < 6; face++)
        {
            faces[face].Init(albedoCubemap.get(), normalCubemap.get(), depthCubeMap.get(), face);
        }
    }
    void Bake(Gfx::CommandBuffer& cmd, DrawList* drawList)
    {
        std::vector<Gfx::ClearValue> clears = {{0, 0, 0, 0}, {0, 0, 0, 0}, {1, 0}};
        for (int face = 0; face < 6; ++face)
        {
            // set scissor and viewport
            Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight)}};
            cmd.SetScissor(0, 1, &scissor);
            Gfx::Viewport viewport{0, 0, static_cast<float>(rtWidth), static_cast<float>(rtHeight), 0, 1};
            cmd.SetViewport(viewport);

            // clear albedo to black
            cmd.BeginRenderPass(*faces[face].gbufferPass, clears);

            if (drawList)
            {
                // draw opaque objects
                for (int i = 0; i < drawList->alphaTestIndex; ++i)
                {
                    auto& draw = drawList->at(i);
                    auto shaderProgram = draw.material->GetShaderProgram("GBuffer");
                    if (shaderProgram)
                    {
                        cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                        cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                        cmd.BindResource(2, draw.shaderResource);
                        cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                        cmd.SetPushConstant(shaderProgram, (void*)&draw.pushConstant);
                        cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                    }
                }

                // draw alpha tested objects
                Shader::EnableFeature("_AlphaTest");
                for (int i = drawList->alphaTestIndex; i < drawList->transparentIndex; ++i)
                {
                    auto& draw = drawList->at(i);
                    auto shaderProgram = draw.material->GetShaderProgram("GBuffer");
                    if (shaderProgram)
                    {
                        cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                        cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                        cmd.BindResource(2, draw.shaderResource);
                        cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                        cmd.SetPushConstant(shaderProgram, (void*)&draw.pushConstant);
                        cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                    }
                }
                Shader::DisableFeature("_AlphaTest");
            }
            cmd.EndRenderPass();
        }

        // project cubemap to octaheral map
        Gfx::ClearValue projectClears[] = {{0, 0, 0, 0}, {1, 0}};
        projectPass.SetAttachment(0, *probe.albedo);
        projectPass.SetAttachment(1, *probe.normal);
        projectPass.SetAttachment(2, *probe.radialDistance);
        cmd.BeginRenderPass(projectPass, projectClears);

        cmd.EndRenderPass();
    }
};
} // namespace Rendering::LFP
