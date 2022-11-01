#include "RenderPipeline.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/ShaderProgram.hpp"

#if GAME_EDITOR
#include "ThirdParty/imgui/imgui.h"
#endif

using namespace Engine::Gfx;
namespace Engine::Rendering
{
    RenderPipeline::RenderPipeline(RefPtr<Gfx::GfxDriver> gfxDriver) : gfxDriver(gfxDriver)
    {
    }
    RenderPipeline::~RenderPipeline() = default;

    void RenderPipeline::Init()
    {
        // create color and depth
        ImageDescription imageDescription{};
        imageDescription.format = ImageFormat::R16G16B16A16_SFloat;
        imageDescription.width = gfxDriver->GetWindowSize().width;
        imageDescription.height = gfxDriver->GetWindowSize().height;
        imageDescription.multiSampling = MultiSampling::Sample_Count_1;
        imageDescription.mipLevels = 1;
        colorImage = Gfx::GfxDriver::Instance()->CreateImage(imageDescription, ImageUsage::ColorAttachment | ImageUsage::TransferSrc | ImageUsage::Texture);

        imageDescription.format = ImageFormat::D24_UNorm_S8_UInt;
        depthImage = Gfx::GfxDriver::Instance()->CreateImage(imageDescription, ImageUsage::DepthStencilAttachment | ImageUsage::TransferSrc);

        // create frameBuffer and renderPass
        renderPass = Gfx::GfxDriver::Instance()->CreateRenderPass();
        RenderPass::Attachment colorAttachment;
        colorAttachment.image = colorImage;
        colorAttachment.multiSampling = MultiSampling::Sample_Count_1;
        colorAttachment.loadOp = AttachmentLoadOperation::Clear;
        colorAttachment.storeOp = AttachmentStoreOperation::Store;

        RenderPass::Attachment depthAttachment;
        depthAttachment.image = depthImage;
        depthAttachment.multiSampling = MultiSampling::Sample_Count_1;
        depthAttachment.loadOp = AttachmentLoadOperation::Clear;
        depthAttachment.storeOp = AttachmentStoreOperation::Store;
        depthAttachment.stencilLoadOp = AttachmentLoadOperation::Clear;
        depthAttachment.stencilStoreOp = AttachmentStoreOperation::Store;
        renderPass->AddSubpass({colorAttachment}, depthAttachment);

        // create global shader resource
        globalResource = Gfx::GfxDriver::Instance()->CreateShaderResource(AssetDatabase::Instance()->GetShader("Internal/SceneLayout")->GetShaderProgram(), Gfx::ShaderResourceFrequency::Global);
    }

    RefPtr<Gfx::Image> RenderPipeline::GetOutputDepth()
    {
        return depthImage;
    }

    void RenderPipeline::Render(RefPtr<GameScene> gameScene)
    {
        Camera* mainCamera = Camera::mainCamera.Get();

        if (mainCamera != nullptr)
        {
            RefPtr<Transform> camTsm = mainCamera->GetGameObject()->GetTransform();

            glm::mat4 viewMatrix = mainCamera->GetViewMatrix();
            glm::mat4 projectionMatrix = mainCamera->GetProjectionMatrix();
            glm::mat4 vp = projectionMatrix * viewMatrix;
            glm::vec3 viewPos = camTsm->GetPosition();
            globalResource->SetUniform("SceneInfo", "view", &viewMatrix);
            globalResource->SetUniform("SceneInfo", "projection", &projectionMatrix);
            globalResource->SetUniform("SceneInfo", "viewProjection", &vp);
            globalResource->SetUniform("SceneInfo", "viewPos", &viewPos);
        }

        auto cmdBuf = gfxDriver->CreateCommandBuffer();

        ProcessLights(gameScene, cmdBuf);

        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;
        clears[1].depthStencil.stencil = 0;
        Rect2D scissor = {{0, 0}, {gfxDriver->GetWindowSize().width, gfxDriver->GetWindowSize().height}};
        cmdBuf->SetScissor(0, 1, &scissor);
        cmdBuf->BindResource(globalResource);
        cmdBuf->BeginRenderPass(renderPass, clears);
        if (gameScene)
        {
            auto& roots = gameScene->GetRootObjects();
            for(auto root : roots)
            {
                RenderObject(root->GetTransform(), cmdBuf);
            }
        }
        cmdBuf->EndRenderPass();
        if (!offscreenOutput)
            cmdBuf->Blit(colorImage, gfxDriver->GetSwapChainImageProxy());

        gfxDriver->ExecuteCommandBuffer(std::move(cmdBuf));
    }

    void RenderPipeline::RenderObject(RefPtr<Transform> transform, UniPtr<CommandBuffer>& cmd)
    {
        for(auto child : transform->GetChildren())
        {
            RenderObject(child, cmd);
        }

        MeshRenderer* meshRenderer = transform->GetGameObject()->GetComponent<MeshRenderer>();
        if (meshRenderer)
        {
            auto mesh = meshRenderer->GetMesh();
            auto material = meshRenderer->GetMaterial();
            auto shader = material ? material->GetShader() : nullptr;
            if (mesh && material && shader)
            {
                // material->SetMatrix("Transform", "model", meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                auto& vertexInfo = mesh->GetVertexDescription();
                Mesh::CommandBindMesh(cmd, mesh);
                cmd->BindResource(material->GetShaderResource());
                cmd->BindShaderProgram(shader->GetShaderProgram(), material->GetShaderConfig());
                auto modelMatrix = meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix();
                cmd->SetPushConstant(shader->GetShaderProgram(), &modelMatrix);
                cmd->DrawIndexed(vertexInfo.index.count, 1, 0, 0, 0);
            }
        }
    }

    RefPtr<Gfx::Image> RenderPipeline::GetOutputColor()
    {
        if (offscreenOutput == true)
        {
            return colorImage;
        }

        return nullptr;
    }

    void RenderPipeline::ProcessLights(RefPtr<GameScene> gameScene, RefPtr<CommandBuffer> cmdBuf)
    {
        auto lights = gameScene->GetActiveLights();
    }
}
