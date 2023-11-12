#include "SceneRenderer.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Importers.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Rendering/RenderGraph/NodeBuilder.hpp"

namespace Engine
{
using namespace RenderGraph;

static void CmdDrawSubmesh(
    Gfx::CommandBuffer& cmd, Mesh& mesh, int submeshIndex, Shader& shader, Gfx::ShaderResource& resource
)
{
    auto& submehses = mesh.GetSubmeshes();
    const Submesh& submesh = submehses[submeshIndex];

    std::vector<Gfx::VertexBufferBinding> bindingds(submesh.GetBindings().size());

    int index = 0;
    for (auto& b : submesh.GetBindings())
    {
        bindingds[index].buffer = submesh.GetVertexBuffer();
        bindingds[index].offset = b.byteOffset;
        index += 1;
    }

    cmd.BindVertexBuffer(bindingds, 0);
    cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
    cmd.BindShaderProgram(shader.GetDefaultShaderProgram(), shader.GetDefaultShaderConfig());
    cmd.BindResource(&resource);
    cmd.DrawIndexed(submesh.GetIndexCount(), 1, 0, 0, 0);
}

SceneRenderer::SceneRenderer(AssetDatabase& db)
{
    shadowShader = (Shader*)db.LoadAsset("_engine_internal/Shaders/Game/ShadowMap.shad");
    opaqueShader = (Shader*)db.LoadAsset("_engine_internal/Shaders/Game/StandardPBR.shad");
    copyOnlyShader = (Shader*)db.LoadAsset("_engine_internal/Shaders/Utils/CopyOnly.shad");
    boxFilterShader = (Shader*)db.LoadAsset("_engine_internal/Shaders/Utils/BoxFilter.shad");

    sceneShaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
        opaqueShader->GetDefaultShaderProgram(),
        Gfx::ShaderResourceFrequency::Global
    );

    globalSceneShaderContentHash = opaqueShader->GetContentHash();

    stagingBuffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Src,
        .size = 1024 * 1024, // 1 MB
        .visibleInCPU = true,
        .debugName = "dual moon graph staging buffer",
    });

    // skybox resources
    envMap = std::make_unique<Texture>("Assets/envCubemap.ktx");
    cube = (Model*)db.LoadAsset("_engine_internal/Models/Cube.glb");
    skyboxShader = std::make_unique<Shader>("Assets/Shaders/Skybox.shad");
    skyboxPassResource =
        GetGfxDriver()->CreateShaderResource(skyboxShader->GetDefaultShaderProgram(), Gfx::ShaderResourceFrequency::Material);
    skyboxPassResource->SetImage("envMap", envMap->GetGfxImage());
    sceneShaderResource->SetImage("environmentMap", envMap->GetGfxImage());
}

void SceneRenderer::ProcessLights(Scene& gameScene)
{
    auto lights = gameScene.GetActiveLights();

    Light* light = nullptr;
    for (int i = 0; i < lights.size(); ++i)
    {
        sceneInfo.lights[i].intensity = lights[i]->GetIntensity();
        auto model = lights[i]->GetGameObject()->GetTransform()->GetModelMatrix();
        sceneInfo.lights[i].position = glm::vec4(-glm::normalize(glm::vec3(model[2])), 0);

        if (lights[i]->GetLightType() == LightType::Directional)
        {
            if (light == nullptr || light->GetIntensity() < lights[i]->GetIntensity())
            {
                light = lights[i];
            }
        }
    }

    sceneInfo.lightCount = glm::vec4(lights.size(), 0, 0, 0);
    if (light)
    {
        sceneInfo.worldToShadow = light->WorldToShadowMatrix();
    }
}

void SceneRenderer::VsmMipMapPass(
    glm::vec2& shadowMapSize,
    std::vector<Gfx::ClearValue>& vsmClears,
    uint32_t& vsmMipLevels,
    RenderNode*& vsmBoxFilterPass1
)
{
    vsmMipmapPasses.clear();
    uint32_t vsmLastMip = vsmMipLevels - 1;
    for (uint32_t mip = 0; mip < vsmLastMip; mip++)
    {
        float mipDownScale = glm::pow(2, mip + 1);
        RenderNode* vsmMipmapPass = AddNode2(
            {PassDependency{
                .name = "vsm source",
                .handle = 0,
                .type = PassDependencyType::Texture,
                .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                .imageSubresourceRange = Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, mip, 1, 0, 1},
            }},
            {
                Subpass{
                    .width = (uint32_t)(shadowMapSize.x / mipDownScale),
                    .height = (uint32_t)(shadowMapSize.y / mipDownScale),
                    .colors =
                        {
                            Attachment{
                                .name = fmt::format("vsm mip {}", mip + 1),
                                .handle = 1,
                                .create = false,
                                .format = Gfx::ImageFormat::R32G32_SFloat,
                                .imageView = ImageView{Gfx::ImageAspect::Color, mip + 1}},
                        },
                },
            },
            [this,
             vsmClears,
             shadowMapSize,
             mip,
             mipDownScale](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& refs)
            {
                uint32_t width = (uint32_t)(shadowMapSize.x / mipDownScale);
                uint32_t height = (uint32_t)(shadowMapSize.y / mipDownScale);
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {width, height}};
                cmd.SetScissor(0, 1, &rect);
                cmd.BeginRenderPass(pass, vsmClears);
                cmd.BindShaderProgram(copyOnlyShader->GetDefaultShaderProgram(), copyOnlyShader->GetDefaultShaderConfig());
                cmd.BindResource(vsmMipShaderResources[mip]);
                float v = (float)mip;
                cmd.SetPushConstant(copyOnlyShader->GetDefaultShaderProgram(), &v);
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();
            }
        );
        vsmMipmapPass->SetName(fmt::format("vsm mip {}", mip + 1));

        vsmMipmapPasses.push_back(vsmMipmapPass);
        if (mip != 0)
        {
            Connect(vsmMipmapPasses[mip - 1], 1, vsmMipmapPasses[mip], 0);
            Connect(vsmMipmapPasses[mip - 1], 1, vsmMipmapPasses[mip], 1);
        }
        else
        {
            Connect(vsmBoxFilterPass1, StrToHandle("dst"), vsmMipmapPasses.front(), 0);
            Connect(vsmBoxFilterPass1, StrToHandle("dst"), vsmMipmapPasses.front(), 1);
        }
    }
}
void SceneRenderer::BuildGraph(const BuildGraphConfig& config)
{
    this->config = config;
    Graph::Clear();
    uint32_t width = config.finalImage->GetDescription().width;
    uint32_t height = config.finalImage->GetDescription().height;

    std::vector<Gfx::ClearValue> shadowClears = {{.depthStencil = {.depth = 1}}};

    Gfx::ShaderResource::BufferMemberInfoMap memberInfo;
    Gfx::Buffer* sceneGlobalBuffer = sceneShaderResource->GetBuffer("SceneInfo", memberInfo).Get();
    float shadowMapSizef = 1024;
    sceneInfo.shadowMapSize = glm::vec4{shadowMapSizef, shadowMapSizef, 1 / shadowMapSizef, 1 / shadowMapSizef};

    auto uploadSceneBuffer = AddNode(
        [this, shadowClears, sceneGlobalBuffer](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& res)
        {
            Camera* camera = scene->GetMainCamera();

            if (camera)
            {
                RefPtr<Transform> camTsm = camera->GetGameObject()->GetTransform();

                glm::mat4 viewMatrix = camera->GetViewMatrix();
                glm::mat4 projectionMatrix = camera->GetProjectionMatrix();
                glm::mat4 vp = projectionMatrix * viewMatrix;
                glm::vec4 viewPos = glm::vec4(camTsm->GetPosition(), 1);
                sceneInfo.projection = projectionMatrix;
                sceneInfo.viewProjection = vp;
                sceneInfo.viewPos = viewPos;
                sceneInfo.view = viewMatrix;
                ProcessLights(*scene);

                size_t copySize = sceneGlobalBuffer->GetSize();
                Gfx::BufferCopyRegion regions[] = {{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = copySize,
                }};

                memcpy(stagingBuffer->GetCPUVisibleAddress(), &sceneInfo, sizeof(sceneInfo));
                cmd.CopyBuffer(stagingBuffer, sceneGlobalBuffer, regions);
            }
        },
        {{
            .name = "global scene buffer",
            .handle = 0,
            .type = ResourceType::Buffer,
            .accessFlags = Gfx::AccessMask::Transfer_Write,
            .stageFlags = Gfx::PipelineStage::Transfer,
            .externalBuffer = sceneShaderResource->GetBuffer("SceneInfo", memberInfo).Get(),
        }} // namespace Engine
        ,
        {}
    );

    auto shadowmapShader = shadowShader;
    glm::vec2 shadowMapSize = sceneInfo.shadowMapSize;

    // variance shadow
    vsmMipShaderResources.clear();
    std::vector<Gfx::ClearValue> vsmClears = {{.color = {{1, 1, 1, 1}}}, {.depthStencil = {1}}};
    uint32_t vsmMipLevels = (uint32_t)glm::floor(glm::log2((float)glm::min(width, height)));
    for (size_t mip = 0; mip < vsmMipLevels - 1; mip++)
    {
        vsmMipShaderResources.push_back(GetGfxDriver()->CreateShaderResource(
            copyOnlyShader->GetDefaultShaderProgram(),
            Gfx::ShaderResourceFrequency::Material
        ));
    }
    vsmPass = AddNode2(
        {
            {
                .name = "global scene buffer",
                .handle = 1,
                .type = PassDependencyType::Buffer,
                .stageFlags = Gfx::PipelineStage::Vertex_Shader | Gfx::PipelineStage::Fragment_Shader,
            },
        },
        {
            {
                .width = (uint32_t)shadowMapSize.x,
                .height = (uint32_t)shadowMapSize.y,
                .colors =
                    {
                        Attachment{
                            .name = "shadow map",
                            .handle = 0,
                            .create = true,
                            .format = Gfx::ImageFormat::R32G32_SFloat,
                            .imageView = ImageView{Gfx::ImageAspect::Color, 0},
                            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                            .mipLevels = vsmMipLevels,
                            .loadOp = Gfx::AttachmentLoadOperation::Clear,
                            .storeOp = Gfx::AttachmentStoreOperation::Store,
                        },
                    },
                .depth =
                    Attachment{
                        .name = "shadow depth",
                        .handle = 2,
                        .create = true,
                        .format = Gfx::ImageFormat::D24_UNorm_S8_UInt,
                        .storeOp = Gfx::AttachmentStoreOperation::DontCare,
                    },
            },
        },
        [this,
         vsmClears,
         shadowmapShader,
         shadowMapSize](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& ref)
        {
            cmd.BindResource(sceneShaderResource);
            cmd.SetViewport(
                {.x = 0, .y = 0, .width = shadowMapSize.x, .height = shadowMapSize.y, .minDepth = 0, .maxDepth = 1}
            );
            Rect2D rect = {{0, 0}, {(uint32_t)shadowMapSize.x, (uint32_t)shadowMapSize.y}};
            cmd.SetScissor(0, 1, &rect);
            cmd.BeginRenderPass(pass, vsmClears);

            for (auto& draw : this->drawList)
            {
                cmd.BindShaderProgram(shadowmapShader->GetDefaultShaderProgram(), shadowmapShader->GetDefaultShaderConfig());
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.SetPushConstant(shadowmapShader->GetDefaultShaderProgram(), &draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }

            cmd.EndRenderPass();
        }
    );
    Connect(uploadSceneBuffer, 0, vsmPass, 1);

    // box filtering vsm
    std::vector<Gfx::ClearValue> boxFilterClears = {{.color = {{1, 1, 1, 1}}}};
    vsmBoxFilterPass0 = AddNode2(
        {
            PassDependency{
                .name = "box filter source",
                .handle = StrToHandle("src"),
                .type = PassDependencyType::Texture,
                .stageFlags = Gfx::PipelineStage::Fragment_Shader,
            },
        },
        {
            Subpass{
                .width = (uint32_t)shadowMapSize.x,
                .height = (uint32_t)shadowMapSize.y,
                .colors =
                    {
                        Attachment{
                            .handle = StrToHandle("dst"),
                            .create = true,
                            .format = Gfx::ImageFormat::R32G32_SFloat,
                        },

                    },
            },
        },
        [=](Gfx::CommandBuffer& cmd, auto& pass, auto& refs)
        {
            uint32_t width = shadowMapSize.x;
            uint32_t height = shadowMapSize.y;
            cmd.SetViewport(
                {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
            );
            Rect2D rect = {{0, 0}, {width, height}};
            cmd.SetScissor(0, 1, &rect);
            cmd.BeginRenderPass(pass, boxFilterClears);
            cmd.BindResource(vsmBoxFilterResource0);
            cmd.BindShaderProgram(boxFilterShader->GetDefaultShaderProgram(), boxFilterShader->GetDefaultShaderConfig());
            struct
            {
                glm::vec4 textureSize;
                glm::vec4 xory;
            } pval;
            pval.textureSize = sceneInfo.shadowMapSize;
            pval.xory = glm::vec4(0);
            cmd.SetPushConstant(boxFilterShader->GetDefaultShaderProgram(), &pval);
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }
    );
    Connect(vsmPass, 0, vsmBoxFilterPass0, StrToHandle("src"));

    auto vsmBoxFilterPass1 = AddNode2(
        {
            PassDependency{
                .name = "box filter source",
                .handle = StrToHandle("src"),
                .type = PassDependencyType::Texture,
                .stageFlags = Gfx::PipelineStage::Fragment_Shader,
            },
        },
        {
            Subpass{
                .width = (uint32_t)shadowMapSize.x,
                .height = (uint32_t)shadowMapSize.y,
                .colors =
                    {
                        Attachment{
                            .handle = StrToHandle("dst"),
                            .create = false,
                            .format = Gfx::ImageFormat::R32G32_SFloat,
                            .imageView = ImageView{Gfx::ImageAspect::Color, 0},
                        },

                    },
            },
        },
        [=](Gfx::CommandBuffer& cmd, auto& pass, auto& refs)
        {
            uint32_t width = shadowMapSize.x;
            uint32_t height = shadowMapSize.y;
            cmd.SetViewport(
                {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
            );
            Rect2D rect = {{0, 0}, {width, height}};
            cmd.SetScissor(0, 1, &rect);
            cmd.BeginRenderPass(pass, boxFilterClears);
            struct
            {
                glm::vec4 textureSize;
                glm::vec4 xory;
            } pval;
            pval.textureSize = sceneInfo.shadowMapSize;
            pval.xory = glm::vec4(1);
            cmd.BindResource(vsmBoxFilterResource1);
            cmd.BindShaderProgram(boxFilterShader->GetDefaultShaderProgram(), boxFilterShader->GetDefaultShaderConfig());
            cmd.SetPushConstant(boxFilterShader->GetDefaultShaderProgram(), &pval);
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }
    );
    Connect(vsmPass, 0, vsmBoxFilterPass1, StrToHandle("dst"));
    Connect(vsmBoxFilterPass0, StrToHandle("dst"), vsmBoxFilterPass1, StrToHandle("src"));

    // down-scale shadow map
    // note: linear mip causes trouble on edge
    // VsmMipMapPass(shadowMapSize, vsmClears, vsmMipLevels, vsmBoxFilterPass1);

    std::vector<Gfx::ClearValue> clearValues = {
        {.color = {{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1}}},
        {.depthStencil = {.depth = 1}}};
    auto forwardOpaque = AddNode2(
        {
            {
                .name = "shadow",
                .handle = RenderGraph::StrToHandle("shadow"),
                .type = RenderGraph::PassDependencyType::Texture,
                .stageFlags = Gfx::PipelineStage::Fragment_Shader,
            },
        },
        {{
            .width = width,
            .height = height,
            .colors = {{
                .name = "opaque color",
                .handle = 0,
                .create = true,
                .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
                .loadOp = Gfx::AttachmentLoadOperation::Clear,
                .storeOp = Gfx::AttachmentStoreOperation::Store,
            }},
            .depth = {{
                .name = "opaque depth",
                .handle = 1,
                .create = true,
                .format = Gfx::ImageFormat::D32_SFLOAT_S8_UInt,
                .loadOp = Gfx::AttachmentLoadOperation::Clear,
                .storeOp = Gfx::AttachmentStoreOperation::Store,
            }},
        }},
        [this, clearValues](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& res)
        {
            Gfx::Image* color = (Gfx::Image*)res.at(0)->GetResource();
            uint32_t width = color->GetDescription().width;
            uint32_t height = color->GetDescription().height;
            cmd.SetViewport(
                {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
            );
            Rect2D rect = {{0, 0}, {width, height}};
            cmd.SetScissor(0, 1, &rect);
            cmd.BindResource(sceneShaderResource);
            cmd.BeginRenderPass(pass, clearValues);

            // draw scene objects
            for (auto& draw : this->drawList)
            {
                cmd.BindShaderProgram(draw.shader, *draw.shaderConfig);
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(draw.shaderResource);
                cmd.SetPushConstant(draw.shader, &draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }

            // draw skybox
            auto& cubeMesh = cube->GetMeshes()[0];
            CmdDrawSubmesh(cmd, *cubeMesh, 0, *skyboxShader, *skyboxPassResource);

            cmd.EndRenderPass();
        }
    );
    Connect(vsmBoxFilterPass1, StrToHandle("dst"), forwardOpaque, RenderGraph::StrToHandle("shadow"));

    auto blitToFinal = AddNode(
        [](Gfx::CommandBuffer& cmd, auto& pass, const ResourceRefs& res)
        {
            Gfx::Image* src = (Gfx::Image*)res.at(1)->GetResource();
            Gfx::Image* dst = (Gfx::Image*)res.at(0)->GetResource();
            cmd.Blit(src, dst);
        },
        {
            {
                .name = "swap chain image",
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Transfer_Write,
                .stageFlags = Gfx::PipelineStage::Transfer,
                .imageUsagesFlags = Gfx::ImageUsage::TransferDst,
                .imageLayout = Gfx::ImageLayout::Transfer_Dst,
                .externalImage = config.finalImage,
            },
            {
                .name = "src",
                .handle = 1,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Transfer_Read,
                .stageFlags = Gfx::PipelineStage::Transfer,
                .imageUsagesFlags = Gfx::ImageUsage::TransferSrc,
                .imageLayout = Gfx::ImageLayout::Transfer_Src,
            },
        },
        {}
    );
    Connect(forwardOpaque, 0, blitToFinal, 1);

    auto layoutTransform = AddNode(
        [](Gfx::CommandBuffer& cmd, auto& pass, const ResourceRefs& res) {},
        {
            {
                .name = "src image",
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = config.accessFlags,
                .stageFlags = config.stageFlags,
                .imageLayout = config.layout,
            },
        },
        {}
    );
    Connect(blitToFinal, 0, layoutTransform, 0);

    colorOutput = blitToFinal;
    colorHandle = 0;
    depthOutput = forwardOpaque;
    depthHandle = 1;
};

void SceneRenderer::AppendDrawData(Transform& transform, std::vector<SceneRenderer::SceneObjectDrawData>& drawList)
{
    for (auto child : transform.GetChildren())
    {
        AppendDrawData(*child, drawList);
    }

    MeshRenderer* meshRenderer = transform.GetGameObject()->GetComponent<MeshRenderer>();
    if (meshRenderer)
    {
        auto mesh = meshRenderer->GetMesh();
        if (mesh == nullptr)
            return;

        auto& submeshes = mesh->GetSubmeshes();
        auto& materials = meshRenderer->GetMaterials();

        for (int i = 0; i < submeshes.size() || i < materials.size(); ++i)
        {
            auto material = i < materials.size() ? materials[i] : nullptr;
            auto submesh = i < submeshes.size() ? &submeshes[i] : nullptr;
            auto shader = material ? material->GetShader() : nullptr;

            if (submesh != nullptr && material != nullptr && shader != nullptr)
            {
                // material->SetMatrix("Transform", "model",
                // meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                uint32_t indexCount = submesh->GetIndexCount();
                SceneObjectDrawData drawData;

                drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                for (auto& binding : submesh->GetBindings())
                {
                    drawData.vertexBufferBinding.push_back({submesh->GetVertexBuffer(), binding.byteOffset});
                }
                drawData.indexBuffer = submesh->GetIndexBuffer();
                drawData.indexBufferType = submesh->GetIndexBufferType();

                drawData.shaderResource = material->GetShaderResource();
                drawData.shader = shader->GetDefaultShaderProgram();
                drawData.shaderConfig = &material->GetShaderConfig();
                auto modelMatrix = meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix();
                drawData.pushConstant = modelMatrix;
                drawData.indexCount = indexCount;
                drawList.push_back(drawData);
            }
        }
    }
}

void SceneRenderer::Render(Gfx::CommandBuffer& cmd, Scene& scene)
{
    this->scene = &scene;
    // culling here?
    if (this->scene)
    {
        auto& roots = this->scene->GetRootObjects();
        drawList.clear();
        for (auto root : roots)
        {
            AppendDrawData(*root->GetTransform(), drawList);
        }

        if (opaqueShader->GetContentHash() != globalSceneShaderContentHash)
        {
            sceneShaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
                opaqueShader->GetDefaultShaderProgram(),
                Gfx::ShaderResourceFrequency::Global
            );
            globalSceneShaderContentHash = opaqueShader->GetContentHash();

            BuildGraph(config);
            Process();

            Gfx::Image* shadowImage = (Gfx::Image*)vsmPass->GetPass()->GetResourceRef(0)->GetResource();
            sceneShaderResource->SetImage("shadowMap", shadowImage);
        }

        Graph::Execute(cmd);
    }
}

void SceneRenderer::Process()
{
    Graph::Process();

    Gfx::Image* shadowImage = (Gfx::Image*)vsmPass->GetPass()->GetResourceRef(0)->GetResource();
    Gfx::Image* vsmBoxFilterPass0Image =
        (Gfx::Image*)vsmBoxFilterPass0->GetPass()->GetResourceRef(StrToHandle("dst"))->GetResource();
    for (uint32_t i = 0; i < vsmMipShaderResources.size(); i++)
    {
        auto& sr = vsmMipShaderResources[i];
        auto imageView = GetGfxDriver()->CreateImageView(Gfx::ImageView::CreateInfo{
            *shadowImage,
            Gfx::ImageViewType::Image_2D,
            {Gfx::ImageAspect::Color, i, 1, 0, 1}});
        sr->SetImage("source", imageView.get());
        vsmMipImageViews.push_back(std::move(imageView));
    }
    sceneShaderResource->SetImage("shadowMap", shadowImage);
    vsmBoxFilterResource0 = GetGfxDriver()->CreateShaderResource(
        boxFilterShader->GetDefaultShaderProgram(),
        Gfx::ShaderResourceFrequency::Material
    );
    vsmBoxFilterResource1 = GetGfxDriver()->CreateShaderResource(
        boxFilterShader->GetDefaultShaderProgram(),
        Gfx::ShaderResourceFrequency::Material
    );
    vsmBoxFilterResource0->SetImage("source", shadowImage);
    vsmBoxFilterResource1->SetImage("source", vsmBoxFilterPass0Image);
}
} // namespace Engine
