#include "SceneRenderer.hpp"
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
    cmd.BindShaderProgram(shader.GetShaderProgram(), shader.GetDefaultShaderConfig());
    cmd.BindResource(&resource);
    cmd.DrawIndexed(submesh.GetIndexCount(), 1, 0, 0, 0);
}

SceneRenderer::SceneRenderer()
{
    shadowShader = shaders.Add("ShadowMap", "Assets/Shaders/Game/ShadowMap.shad");
    opaqueShader = shaders.Add("StandardPBR", "Assets/Shaders/Game/StandardPBR.shad");
    sceneShaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
        opaqueShader->GetShaderProgram(),
        Gfx::ShaderResourceFrequency::Global
    );
    stagingBuffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Src,
        .size = 1024 * 1024, // 1 MB
        .visibleInCPU = true,
        .debugName = "dual moon graph staging buffer",
    });

    // skybox resources
    envMap = std::make_unique<Texture>("Assets/envMap.ktx");
    cube = Importers::GLB("Assets/cube.glb", skyboxShader.get());
    skyboxShader = std::make_unique<Shader>("Assets/Shaders/Skybox.shad");
    skyboxPassResource =
        GetGfxDriver()->CreateShaderResource(skyboxShader->GetShaderProgram(), Gfx::ShaderResourceFrequency::Material);
    skyboxPassResource->SetTexture("envMap", envMap->GetGfxImage());
    sceneShaderResource->SetTexture("environmentMap", envMap->GetGfxImage());
}

void SceneRenderer::ProcessLights(Scene& gameScene)
{
    auto lights = gameScene.GetActiveLights();

    Light* light = nullptr;
    for (int i = 0; i < lights.size(); ++i)
    {
        sceneInfo.lights[i].intensity = lights[i]->GetIntensity();
        auto model = lights[i]->GetGameObject()->GetTransform()->GetModelMatrix();
        sceneInfo.lights[i].position = glm::vec4(glm::normalize(glm::vec3(model[2])), 0);

        if (lights[i]->GetLightType() == LightType::Directional)
        {
            if (light == nullptr || light->GetIntensity() < lights[i]->GetIntensity())
            {
                light = lights[i].Get();
            }
        }
    }

    sceneInfo.lightCount = glm::vec4(lights.size(), 0, 0, 0);
    if (light)
    {
        sceneInfo.worldToShadow = light->WorldToShadowMatrix();
    }
}

void SceneRenderer::BuildGraph(const BuildGraphConfig& config)
{
    Graph::Clear();
    uint32_t width = config.finalImage.GetDescription().width;
    uint32_t height = config.finalImage.GetDescription().height;

    std::vector<Gfx::ClearValue> shadowClears = {{.depthStencil = {.depth = 1}}};

    Gfx::ShaderResource::BufferMemberInfoMap memberInfo;
    Gfx::Buffer* sceneGlobalBuffer = sceneShaderResource->GetBuffer("SceneInfo", memberInfo).Get();

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

                Gfx::BufferCopyRegion regions[] = {{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = sceneGlobalBuffer->GetSize(),
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
    shadowPass = AddNode(
        [this, shadowClears, shadowmapShader](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& res)
        {
            cmd.BindResource(sceneShaderResource);
            cmd.SetViewport({.x = 0, .y = 0, .width = 1024, .height = 1024, .minDepth = 0, .maxDepth = 1});
            Rect2D rect = {{0, 0}, {1024, 1024}};
            cmd.SetScissor(0, 1, &rect);
            cmd.BeginRenderPass(pass, shadowClears);

            for (auto& draw : this->drawList)
            {
                cmd.BindShaderProgram(shadowmapShader->GetShaderProgram(), shadowmapShader->GetDefaultShaderConfig());
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.SetPushConstant(shadowmapShader->GetShaderProgram(), &draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
            cmd.EndRenderPass();
        },
        {
            {
                .name = "shadow",
                .handle = 0,
                .type = ResourceType::Image,
                .accessFlags =
                    Gfx::AccessMask::Depth_Stencil_Attachment_Write | Gfx::AccessMask::Depth_Stencil_Attachment_Read,
                .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests | Gfx::PipelineStage::Late_Fragment_Tests,
                .imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment,
                .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
                .imageCreateInfo =
                    {
                        .width = 1024,
                        .height = 1024,
                        .format = Gfx::ImageFormat::D16_UNorm,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                        .isCubemap = false,
                    },
            },
            {
                .name = "global scene buffer",
                .handle = 1,
                .type = ResourceType::Buffer,
                .accessFlags = Gfx::AccessMask::Shader_Read,
                .stageFlags = Gfx::PipelineStage::Vertex_Shader | Gfx::PipelineStage::Fragment_Shader,
            },
        },
        {{
            .depth = {{
                .handle = 0,
                .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                .loadOp = Gfx::AttachmentLoadOperation::Clear,
                .storeOp = Gfx::AttachmentStoreOperation::Store,
            }},
        }}
    );
    Connect(uploadSceneBuffer, 0, shadowPass, 1);

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
        // {
        //     {
        //         .name = "opaque color",
        //         .handle = 0,
        //         .type = ResourceType::Image,
        //         .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
        //         .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
        //         .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
        //         .imageLayout = Gfx::ImageLayout::Color_Attachment,
        //         .imageCreateInfo =
        //             {
        //                 .width = width,
        //                 .height = height,
        //                 .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
        //                 .multiSampling = Gfx::MultiSampling::Sample_Count_1,
        //                 .mipLevels = 1,
        //                 .isCubemap = false,
        //             },
        //     },
        //     {
        //         .name = "opaque depth",
        //         .handle = 1,
        //         .type = ResourceType::Image,
        //         .accessFlags =
        //             Gfx::AccessMask::Depth_Stencil_Attachment_Read | Gfx::AccessMask::Depth_Stencil_Attachment_Write,
        //         .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests,
        //         .imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment,
        //         .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
        //         .imageCreateInfo =
        //             {
        //                 .width = width,
        //                 .height = height,
        //                 .format = Gfx::ImageFormat::D32_SFLOAT_S8_UInt,
        //                 .multiSampling = Gfx::MultiSampling::Sample_Count_1,
        //                 .mipLevels = 1,
        //                 .isCubemap = false,
        //             },
        //     },
        //     {
        //         .name = "shadow",
        //         .handle = 2,
        //         .type = ResourceType::Image,
        //         .accessFlags = Gfx::AccessMask::Shader_Read,
        //         .stageFlags = Gfx::PipelineStage::Fragment_Shader,
        //         .imageUsagesFlags = Gfx::ImageUsage::Texture,
        //         .imageLayout = Gfx::ImageLayout::Shader_Read_Only,
        //     },
        // },
        // {
        //     {
        //         .colors =
        //             {
        //                 {
        //                     .handle = 0,
        //                     .multiSampling = Gfx::MultiSampling::Sample_Count_1,
        //                     .loadOp = Gfx::AttachmentLoadOperation::Clear,
        //                     .storeOp = Gfx::AttachmentStoreOperation::Store,
        //                 },
        //             },
        //         .depth = {{
        //             .handle = 1,
        //             .multiSampling = Gfx::MultiSampling::Sample_Count_1,
        //             .loadOp = Gfx::AttachmentLoadOperation::Clear,
        //             .storeOp = Gfx::AttachmentStoreOperation::Store,
        //         }},
        //     },
        // }
    );
    Connect(shadowPass, 0, forwardOpaque, RenderGraph::StrToHandle("shadow"));

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
                .externalImage = &config.finalImage,
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

                drawData.shaderResource = material->GetShaderResource().Get();
                drawData.shader = shader->GetShaderProgram().Get();
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

        Graph::Execute(cmd);
    }
}

void SceneRenderer::Process()
{
    Graph::Process();

    Gfx::Image* shadowImage = (Gfx::Image*)shadowPass->GetPass()->GetResourceRef(0)->GetResource();
    sceneShaderResource->SetTexture("shadowMap", shadowImage);
}
} // namespace Engine
