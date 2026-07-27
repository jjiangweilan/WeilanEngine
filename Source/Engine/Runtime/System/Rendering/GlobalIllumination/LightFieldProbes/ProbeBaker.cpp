#include "ProbeBaker.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Editor/MainMenuModule.hpp"

namespace Rendering::LFP
{
ProbeBaker::ProbeBaker(Probe& probe) : probe(&probe)
{
    albedoCubemap = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            rtWidth,
            rtHeight,
            1,
            Gfx::GfxFormat::R8G8B8A8_SRGB,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            true
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
    );
    albedoCubemap->SetName("lightFieldAlbedoCubemap");

    normalCubemap = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            rtWidth,
            rtHeight,
            1,
            Gfx::GfxFormat::A2B10G10R10_UNorm,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            true
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
    );
    normalCubemap->SetName("lightFieldNormalCubemap");

    depthCubeMap = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            rtWidth,
            rtHeight,
            1,
            Gfx::GfxFormat::D32_SFloat,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            true
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::DepthStencilAttachment
    );
    depthCubeMap->SetName("lightFieldDepthCubemap");

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

    Gfx::Attachment radialDistance{
        &probe.radialDistance->GetDefaultImageView(),
        Gfx::MultiSampling::Sample_Count_1,
        Gfx::AttachmentLoadOperation::Clear
    };
}

static void DispatchBake(Gfx::CommandBuffer& cmd, DrawList*& drawList, int from, int to)
{
    for (int i = from; i < to; ++i)
    {
        auto& draw = drawList->at(i);
        auto shaderProgram = draw.material->GetShaderProgram();
        if (shaderProgram)
        {
            cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
            cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
            cmd.BindResource(2, draw.materialResource);
            if (draw.objectResource)
            {
                cmd.BindResource(3, draw.objectResource);
            }
            cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultPipelineConfig());
            auto ps = draw.GetPushConstant();
            cmd.SetPushConstant(shaderProgram, (void*)&ps);
            cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
        }
    }
}

void ProbeBaker::Bake(Gfx::CommandBuffer& cmd, DrawList* drawList)
{
    std::vector<Gfx::ClearValue> clears = {{0, 0, 0, 0}, {0, 0, 0, 0}, {1, 0}};
    for (int face = 0; face < 6; ++face)
    {
        // clear albedo to black
        cmd.BeginRenderPass(*faces[face].gbufferPass, clears);

        cmd.BindResource(1, faces[face].set1Resource.get());
        if (drawList)
        {
            // draw opaque objects
            DispatchBake(cmd, drawList, 0, drawList->alphaTestIndex);
            // draw alpha tested objects
            // Obsolete::Shader::EnableFeature("_AlphaClip");
            DispatchBake(cmd, drawList, drawList->alphaTestIndex, drawList->transparentIndex);
            // Obsolete::Shader::DisableFeature("_AlphaClip");
        }
        cmd.EndRenderPass();
    }

    // project cubemap to octaheral map
    Gfx::ClearValue projectClears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    cmd.BeginRenderPass(*probeOctahedralPass, projectClears);
    cmd.BindResource(2, reprojectMaterial.GetShaderResource());
    cmd.BindShaderProgram(
        octahedralRemapShader->GetShaderProgram(),
        octahedralRemapShader->GetShaderProgram()->GetDefaultPipelineConfig()
    );
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();

    probe->baked = true;
}

Shader* ProbeBaker::GetOctahedralRemapBaker()
{
    return nullptr;
    //static Obsolete::Shader* octahedralRemapBakerShader = nullptr;
    //if (!octahedralRemapBakerShader)
    //{
    //    octahedralRemapBakerShader = (Obsolete::Shader*)AssetDatabase::Singleton()->LoadAsset(
    //        "_engine_internal/Shaders/LightFieldProbes/OctahedralRemapBaker.shad"
    //    );
    //}
    //return octahedralRemapBakerShader;
}

REGISTER_MAIN_MENU_ITEM("Editor/ProbeBaker") {}
} // namespace Rendering::LFP
