#include "GlobalIllumination.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/DrawList.hpp"
namespace Rendering::GI
{

struct Probe
{
    void Relight(Gfx::CommandBuffer& cmd);

    glm::float3 position;

    // octahedral maps

    std::unique_ptr<Gfx::Image> lighted;

    static const uint32_t octahedralMapSize = 128;
};

struct ProbeFace
{
    std::unique_ptr<Gfx::RenderPass> gbufferPass;
    std::unique_ptr<Gfx::ImageView> albedoView;
    std::unique_ptr<Gfx::ImageView> normalView;
    std::unique_ptr<Gfx::ImageView> depthView;
    std::unique_ptr<Gfx::Buffer> lfpBuffer;
    std::unique_ptr<Gfx::ShaderResource> set1Resource;
    glm::mat4 vp;

    void Init(Gfx::Image* albedoCubemap, Gfx::Image* normalCubemap, Gfx::Image* depthCubeMap, uint32_t face)
    {
        gbufferPass = GetGfxDriver()->CreateRenderPass();
        albedoView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *albedoCubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, face, 1}
        });

        normalView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *normalCubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, face, 1}
        });

        depthView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *depthCubeMap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{Gfx::ImageAspect::Depth, 0, 1, face, 1}
        });

        Gfx::Attachment albedoAttachment{
            albedoView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::Attachment normalAttachment{
            normalView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::Attachment depthAttachment{
            depthView.get(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };

        gbufferPass->AddSubpass({albedoAttachment, normalAttachment}, depthAttachment);
        lfpBuffer = GetGfxDriver()->CreateBuffer(
            Gfx::Buffer::CreateInfo{Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst, 64, false, "", false}
        );

        glm::vec3 cubeDir[6] = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f)
        };

        glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(90.0f), 1.0f, 0.01f, 100.0f);
        projection[1] = -projection[1];
        glm::mat4 view = glm::lookAtLH(
            glm::vec3(0),
            cubeDir[face],
            (face != 2 && face != 3) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f)
        );
        vp = projection * view;
        GetGfxDriver()->UploadBuffer(*lfpBuffer, (uint8_t*)&vp, sizeof(vp));
        set1Resource = GetGfxDriver()->CreateShaderResource();
        set1Resource->SetBuffer("LFP", lfpBuffer.get());
    };
};

class ProbeBaker
{
public:
    // the probe to bake to
    ProbeBaker()
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
            faces[face].Init(albedoCubemap.get(), normalCubemap.get(), depthCubeMap.get(), face);
        }

        probeOctahedralPass = GetGfxDriver()->CreateRenderPass();
        Gfx::Attachment albedoAtta{
            &albedoOctahedralAtlas->GetDefaultImageView(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear
        };

        Gfx::Attachment normalAtta{
            &normalOctahedralAtlas->GetDefaultImageView(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear
        };

        Gfx::Attachment radialDistance{
            &radialDistanceOctahedralAtlas->GetDefaultImageView(),
            Gfx::MultiSampling::Sample_Count_1,
            Gfx::AttachmentLoadOperation::Clear
        };
    }

    void Bake(Gfx::CommandBuffer& cmd, DrawList* drawList, std::vector<Probe>& outProbes)
    {
        std::vector<ProbeInfo> probeInfos(outProbes.size());
        for (int i = 0; i < outProbes.size(); ++i)
        {
            probeInfos[i].positionWS = glm::float4(outProbes[i].position, 1.0);
        }

        for (auto& probe : outProbes)
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
                    DispatchBake(cmd, drawList, drawList->alphaTestIndex, drawList->transparentIndex);
                }
                cmd.EndRenderPass();
            }

            // project cubemap to octaheral map
            Gfx::ClearValue projectClears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
            cmd.BeginRenderPass(*probeOctahedralPass, projectClears);
            cmd.BindResource(2, reprojectMaterial.GetShaderResource());
            cmd.BindShaderProgram(
                octahedralRemapShader->GetDefaultShaderProgram(),
                octahedralRemapShader->GetDefaultShaderConfig()
            );
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }
    }

    std::span<ProbeFace> GetFaces() { return faces; }

    std::unique_ptr<Gfx::Image>& GetAlbedoCubemap() { return albedoCubemap; }

    std::unique_ptr<Gfx::Image>& GetNormalCubemap() { return normalCubemap; }

private:
    ProbeFace faces[6];
    std::unique_ptr<Gfx::Image> albedoCubemap;
    std::unique_ptr<Gfx::Image> normalCubemap;
    std::unique_ptr<Gfx::Image> depthCubeMap;
    std::unique_ptr<Gfx::Image> albedoOctahedralAtlas;
    std::unique_ptr<Gfx::Image> normalOctahedralAtlas;
    std::unique_ptr<Gfx::Image> radialDistanceOctahedralAtlas;
    struct ProbeInfo
    {
        glm::float4 positionWS;
    };
    std::unique_ptr<Gfx::Buffer> probeInfos;
    Material reprojectMaterial;

    const uint32_t rtWidth = 128;
    const uint32_t rtHeight = 128;

    std::unique_ptr<Gfx::RenderPass> probeOctahedralPass;
    Probe* probe;
    Obsolete::Shader* probeCubemapShader;
    Obsolete::Shader* octahedralRemapShader;

    Obsolete::Shader* GetOctahedralRemapBaker();

    void DispatchBake(Gfx::CommandBuffer& cmd, DrawList*& drawList, int from, int to)
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
                cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                auto ps = draw.GetPushConstant();
                cmd.SetPushConstant(shaderProgram, (void*)&ps);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
        }
    }
};

void GlobalIlluminaion::PreprocessScene(Scene& scene, const BakeProbesInfo& info)
{
    // collect all suitable renderers
    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    DrawList drawList;
    for (auto go : scene.GetAllGameObjects())
    {
        auto m = go->GetComponent<MeshRenderer>();
        if (m)
        {
            drawList.Add(*m);
        }
    }

    //
}

const std::string& GIScene::GetName()
{
    static std::string name = "GIScene";
    return name;
}

void GIScene::Serialize(Serializer* s) const {}
void GIScene::Deserialize(Serializer* s) {}

void GIScene::PrebakeScene()
{
    auto go = GetGameObject();
    Scene* scene = go ? go->GetScene() : nullptr;
    if (!scene)
        return;
}

std::unique_ptr<Component> GIScene::Clone(GameObject& owner)
{
    return nullptr;
}
} // namespace Rendering::GI
