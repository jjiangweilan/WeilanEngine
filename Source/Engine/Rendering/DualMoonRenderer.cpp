#include "DualMoonRenderer.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Rendering/RenderGraph/NodeBuilder.hpp"

namespace Engine
{
using namespace RenderGraph;

DualMoonRenderer::DualMoonRenderer(SceneManager& sceneManager) : sceneManager(sceneManager)
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

    BuildGraph();
}

void DualMoonRenderer::ProcessLights(Scene* gameScene)
{
    auto lights = gameScene->GetActiveLights();

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

void DualMoonRenderer::RebuildGraph()
{
    Graph::Clear();
    BuildGraph();
}

void DualMoonRenderer::BuildGraph()
{
    auto swapChainProxy = GetGfxDriver()->GetSwapChainImageProxy().Get();
    uint32_t width = swapChainProxy->GetDescription().width;
    uint32_t height = swapChainProxy->GetDescription().height;

    std::vector<Gfx::ClearValue> shadowClears = {{.depthStencil = {.depth = 1}}};

    Gfx::ShaderResource::BufferMemberInfoMap memberInfo;
    Gfx::Buffer* sceneGlobalBuffer = sceneShaderResource->GetBuffer("SceneInfo", memberInfo).Get();
    auto uploadSceneBuffer = AddNode(
        [this, shadowClears, sceneGlobalBuffer](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& res)
        {
            Scene* scene = sceneManager.GetActiveScene();

            if (scene)
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
                    ProcessLights(scene);

                    Gfx::BufferCopyRegion regions[] = {{
                        .srcOffset = 0,
                        .dstOffset = 0,
                        .size = sceneGlobalBuffer->GetSize(),
                    }};

                    memcpy(stagingBuffer->GetCPUVisibleAddress(), &sceneInfo, sizeof(sceneInfo));
                    cmd.CopyBuffer(stagingBuffer, sceneGlobalBuffer, regions);
                }
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
            cmd.BeginRenderPass(&pass, shadowClears);

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
        {.color = {3 / 255.0f, 190 / 255.0f, 252 / 255.0f, 1}},
        {.depthStencil = {.depth = 1}}};
    auto forwardOpaque = AddNode(
        [this, clearValues](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& res)
        {
            cmd.BindResource(sceneShaderResource);
            cmd.BeginRenderPass(&pass, clearValues);
            for (auto& draw : this->drawList)
            {
                cmd.BindShaderProgram(draw.shader, *draw.shaderConfig);
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(draw.shaderResource);
                cmd.SetPushConstant(draw.shader, &draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
            cmd.EndRenderPass();
        },
        {
            {
                .name = "opaque color",
                .handle = 0,
                .type = ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
                .imageCreateInfo =
                    {
                        .width = width,
                        .height = height,
                        .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                        .isCubemap = false,
                    },
            },
            {
                .name = "opaque depth",
                .handle = 1,
                .type = ResourceType::Image,
                .accessFlags =
                    Gfx::AccessMask::Depth_Stencil_Attachment_Read | Gfx::AccessMask::Depth_Stencil_Attachment_Write,
                .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests,
                .imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment,
                .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
                .imageCreateInfo =
                    {
                        .width = width,
                        .height = height,
                        .format = Gfx::ImageFormat::D32_SFLOAT_S8_UInt,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                        .isCubemap = false,
                    },
            },
            {
                .name = "shadow",
                .handle = 2,
                .type = ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Shader_Read,
                .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                .imageUsagesFlags = Gfx::ImageUsage::Texture,
                .imageLayout = Gfx::ImageLayout::Shader_Read_Only,
            },
        },
        {
            {
                .colors =
                    {
                        {
                            .handle = 0,
                            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                            .loadOp = Gfx::AttachmentLoadOperation::Clear,
                            .storeOp = Gfx::AttachmentStoreOperation::Store,
                        },
                    },
                .depth = {{
                    .handle = 1,
                    .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                    .loadOp = Gfx::AttachmentLoadOperation::Clear,
                    .storeOp = Gfx::AttachmentStoreOperation::Store,
                }},
            },
        }
    );
    Connect(shadowPass, 0, forwardOpaque, 2);

    auto blitToSwapchain = AddNode(
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
                .externalImage = swapChainProxy,
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
    Connect(forwardOpaque, 0, blitToSwapchain, 1);

    colorOutput = blitToSwapchain;
    colorHandle = 0;
    depthOutput = forwardOpaque;
    depthHandle = 1;
};

void DualMoonRenderer::AppendDrawData(
    Transform& transform, std::vector<DualMoonRenderer::SceneObjectDrawData>& drawList
)
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

        auto& submeshes = mesh->submeshes;
        auto& materials = meshRenderer->GetMaterials();

        for (int i = 0; i < submeshes.size() || i < materials.size(); ++i)
        {
            auto material = i < materials.size() ? materials[i] : nullptr;
            auto submesh = i < submeshes.size() ? submeshes[i].get() : nullptr;
            auto shader = material ? material->GetShader() : nullptr;

            if (submesh && material && shader)
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

void DualMoonRenderer::Execute(Gfx::CommandBuffer& cmd)
{
    // culling here?
    auto scene = sceneManager.GetActiveScene();

    if (scene)
    {
        auto& roots = scene->GetRootObjects();
        drawList.clear();
        for (auto root : roots)
        {
            AppendDrawData(*root->GetTransform(), drawList);
        }

        Graph::Execute(cmd);
    }
}

void DualMoonRenderer::Process()
{
    Graph::Process();

    Gfx::Image* shadowImage = (Gfx::Image*)shadowPass->GetPass()->GetResourceRef(0)->GetResource();
    sceneShaderResource->SetTexture("shadowMap", shadowImage);
}
} // namespace Engine