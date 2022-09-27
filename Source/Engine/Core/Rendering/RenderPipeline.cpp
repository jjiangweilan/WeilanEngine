#include "RenderPipeline.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Globals.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "Core/Graphics/RenderContext.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "GfxDriver/GfxFactory.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/ShaderProgram.hpp"

#if GAME_EDITOR
#include "Editor/imgui/imgui.h"
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
        renderContext = gfxDriver->GetRenderContext();

        // create color and depth
        ImageDescription imageDescription{};
        imageDescription.format = ImageFormat::R16G16B16A16_SFloat;
        imageDescription.width = gfxDriver->GetWindowSize().width;
        imageDescription.height = gfxDriver->GetWindowSize().height;
        imageDescription.multiSampling = MultiSampling::Sample_Count_1;
        imageDescription.mipLevels = 1;
        colorImage = Gfx::GfxFactory::Instance()->CreateImage(imageDescription, ImageUsage::ColorAttachment | ImageUsage::TransferSrc | ImageUsage::Texture);

        imageDescription.format = ImageFormat::D24_UNorm_S8_UInt;
        depthImage = Gfx::GfxFactory::Instance()->CreateImage(imageDescription, ImageUsage::DepthStencilAttachment | ImageUsage::TransferSrc);

        // create frameBuffer and renderPass
        renderPass = Gfx::GfxFactory::Instance()->CreateRenderPass();
        RenderPass::Attachment colorAttachment;
        colorAttachment.format = ImageFormat::R16G16B16A16_SFloat;
        colorAttachment.multiSampling = MultiSampling::Sample_Count_1;
        colorAttachment.loadOp = AttachmentLoadOperation::Clear;
        colorAttachment.storeOp = AttachmentStoreOperation::Store;

        RenderPass::Attachment depthAttachment;
        depthAttachment.format = ImageFormat::D24_UNorm_S8_UInt;
        depthAttachment.multiSampling = MultiSampling::Sample_Count_1;
        depthAttachment.loadOp = AttachmentLoadOperation::Clear;
        depthAttachment.storeOp = AttachmentStoreOperation::Store;
        depthAttachment.stencilLoadOp = AttachmentLoadOperation::Clear;
        depthAttachment.stencilStoreOp = AttachmentStoreOperation::Store;
        renderPass->SetAttachments({colorAttachment}, depthAttachment);
        RenderPass::Subpass subpass;
        subpass.colors.push_back(0);
        subpass.depthAttachment = 1;
        renderPass->SetSubpass({subpass});
        frameBuffer = Gfx::GfxFactory::Instance()->CreateFrameBuffer(renderPass);
        frameBuffer->SetAttachments({colorImage, depthImage});
        gfxDriver->SetPresentImage(colorImage);

        // create global shader resource
        globalResource = Gfx::GfxFactory::Instance()->CreateShaderResource(AssetDatabase::Instance()->GetShader("Internal/SceneLayout")->GetShaderProgram(), Gfx::ShaderResourceFrequency::View);
    }

    RefPtr<Gfx::Image> RenderPipeline::GetOutputDepth()
    {
        return depthImage;
    }

    void RenderPipeline::Render(RefPtr<GameScene> gameScene)
    {
        Camera* mainCamera = Globals::GetMainCamera();
        if (mainCamera != nullptr)
        {
            RefPtr<Transform> camTsm = mainCamera->GetGameObject()->GetTransform();

            glm::mat4 viewMatrix = mainCamera->GetViewMatrix();
            glm::mat4 projectionMatrix = mainCamera->GetProjectionMatrix();
            glm::mat4 vp = projectionMatrix * viewMatrix;
            globalResource->SetUniform("SceneInfo", "view", &viewMatrix);
            globalResource->SetUniform("SceneInfo", "projection", &projectionMatrix);
            globalResource->SetUniform("SceneInfo", "viewProjection", &vp);
        }

        auto cmdBuf = renderContext->CreateCommandBuffer();

        renderContext->BeginFrame(globalResource);

        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;
        clears[1].depthStencil.stencil = 0;
        Rect2D scissor = {{0, 0}, {gfxDriver->GetWindowSize().width, gfxDriver->GetWindowSize().height}};
        cmdBuf->SetScissor(0, 1, &scissor);
        cmdBuf->BeginRenderPass(renderPass, frameBuffer, clears);
        if (gameScene)
        {
            auto& roots = gameScene->GetRootObjects();
            for(auto root : roots)
            {
                RenderObject(root->GetTransform(), cmdBuf);
            }
        }
        cmdBuf->EndRenderPass();
        renderContext->EndFrame();

        renderContext->ExecuteCommandBuffer(std::move(cmdBuf));
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

            if (mesh && material)
            {
                material->SetMatrix("Transform", "model", meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                cmd->Render(*mesh, *material);
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
}
