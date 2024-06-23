#include "ProbeBaker.hpp"
#include "Editor/EditorTickModule.hpp"
#include "Editor/MainMenuModule.hpp"

namespace Rendering::LFP
{
ProbeBaker::ProbeBaker(Probe& probe, Shader* ProbeCubemapBaker, Shader* octahedralRemapBaker)
    : probe(&probe), probeCubemapShader(ProbeCubemapBaker), octahedralRemapShader(octahedralRemapBaker)
{
    for (int face = 0; face < 6; face++)
    {
        faces[face].Init(albedoCubemap.get(), normalCubemap.get(), depthCubeMap.get(), face, probe.position);
    }

    probeOctahedralPass = GetGfxDriver()->CreateRenderPass();
    Gfx::Attachment albedoAtta{
        &probe.albedo->GetDefaultImageView(),
        Gfx::MultiSampling::Sample_Count_1,
        Gfx::AttachmentLoadOperation::Clear
    };

    Gfx::Attachment normalAtta{
        &probe.normal->GetDefaultImageView(),
        Gfx::MultiSampling::Sample_Count_1,
        Gfx::AttachmentLoadOperation::Clear
    };

    Gfx::Attachment depthAtta{
        &probe.radialDistance->GetDefaultImageView(),
        Gfx::MultiSampling::Sample_Count_1,
        Gfx::AttachmentLoadOperation::Clear
    };

    probeOctahedralPass->AddSubpass({albedoAtta, normalAtta, depthAtta}, std::nullopt);
}

static void DispatchBake(Gfx::CommandBuffer& cmd, DrawList*& drawList, int from, int to)
{
    for (int i = from; i < to; ++i)
    {
        auto& draw = drawList->at(i);
        auto shaderProgram = draw.material->GetShaderProgram("LightFieldProbeBake");
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
}
void ProbeBaker::Bake(Gfx::CommandBuffer& cmd, DrawList* drawList)
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

        cmd.BindResource(1, faces[face].set1Resource.get());
        if (drawList)
        {
            // draw opaque objects
            DispatchBake(cmd, drawList, 0, drawList->alphaTestIndex);
            // draw alpha tested objects
            Shader::EnableFeature("_AlphaTest");
            DispatchBake(cmd, drawList, drawList->alphaTestIndex, drawList->transparentIndex);
            Shader::DisableFeature("_AlphaTest");
        }
        cmd.EndRenderPass();
    }

    // project cubemap to octaheral map
    Gfx::ClearValue projectClears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    cmd.BeginRenderPass(*probeOctahedralPass, projectClears);
    cmd.BindShaderProgram(
        octahedralRemapShader->GetDefaultShaderProgram(),
        octahedralRemapShader->GetDefaultShaderConfig()
    );
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

REGISTER_MAIN_MENU_ITEM("Editor/ProbeBaker") {}
} // namespace Rendering::LFP
