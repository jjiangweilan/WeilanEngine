#include "RenderPipeline.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/GfxResourceTransfer.hpp"

#if GAME_EDITOR
#include "ThirdParty/imgui/imgui.h"
#endif

using namespace Engine::Gfx;
using namespace Engine::RGraph;
namespace Engine::Rendering
{
RenderPipeline::RenderPipeline(RefPtr<Gfx::GfxDriver> gfxDriver) : gfxDriver(gfxDriver) {}
RenderPipeline::~RenderPipeline() {}

void RenderPipeline::Init()
{
    // create color and depth
    ImageDescription imageDescription{};
    imageDescription.format = ImageFormat::R16G16B16A16_SFloat;
    imageDescription.width = gfxDriver->GetWindowSize().width;
    imageDescription.height = gfxDriver->GetWindowSize().height;
    imageDescription.multiSampling = MultiSampling::Sample_Count_1;
    imageDescription.mipLevels = 1;
    colorImage = Gfx::GfxDriver::Instance()->CreateImage(
        imageDescription, ImageUsage::ColorAttachment | ImageUsage::TransferSrc | ImageUsage::Texture);

    imageDescription.format = ImageFormat::D24_UNorm_S8_UInt;
    depthImage = Gfx::GfxDriver::Instance()->CreateImage(imageDescription,
                                                         ImageUsage::DepthStencilAttachment | ImageUsage::TransferSrc);

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

    mainQueue = gfxDriver->GetQueue(QueueType::Main);
    imageAcquireSemaphore = GetGfxDriver()->CreateSemaphore({false});
    cmdPool = gfxDriver->CreateCommandPool({mainQueue});
    cmdBuf = std::move(cmdPool->AllocateCommandBuffers(CommandBufferType::Primary, 1).at(0));

    // build render graph
    colorImageNode = graph.AddNode<RGraph::ImageNode>();
    colorImageNode->width = gfxDriver->GetWindowSize().width;
    colorImageNode->height = gfxDriver->GetWindowSize().height;
    colorImageNode->mipLevels = 1;
    colorImageNode->multiSampling = MultiSampling::Sample_Count_1;
    colorImageNode->format = ImageFormat::R16G16B16A16_SFloat;

    depthImageNode = graph.AddNode<RGraph::ImageNode>();
    depthImageNode->width = colorImageNode->width;
    depthImageNode->height = colorImageNode->height;
    depthImageNode->mipLevels = colorImageNode->mipLevels;
    depthImageNode->multiSampling = colorImageNode->multiSampling;
    depthImageNode->format = ImageFormat::D24_UNorm_S8_UInt;

    renderPassBeginNode = graph.AddNode<RGraph::RenderPassBeginNode>();
    renderPassBeginNode->SetColorCount(1);
    renderPassBeginNode->GetColorAttachmentOps()[0].loadOp = AttachmentLoadOperation::Clear;
    renderPassBeginNode->GetColorAttachmentOps()[0].storeOp = AttachmentStoreOperation::Store;
    renderPassBeginNode->GetColorAttachmentOps()[0].stencilLoadOp = AttachmentLoadOperation::Clear;
    renderPassBeginNode->GetColorAttachmentOps()[0].stencilStoreOp = AttachmentStoreOperation::Store;

    renderPassEndNode = graph.AddNode<RGraph::RenderPassEndNode>();

    commandBufferBeginNode = graph.AddNode<CommandBufferBeginNode>();
    commandBufferEndNode = graph.AddNode<CommandBufferEndNode>();

    //    graph.Compile();
}

void RenderPipeline::Render(RefPtr<GameScene> gameScene)
{
    gfxDriver->AcquireNextSwapChainImage(imageAcquireSemaphore);
    gfxDriver->WaitForFence({mainCmdBufFinishedFence}, true, -1);
    cmdPool->ResetCommandPool();

    Camera* mainCamera = Camera::mainCamera.Get();
    if (mainCamera != nullptr)
    {
        RefPtr<Transform> camTsm = mainCamera->GetGameObject()->GetTransform();

        glm::mat4 viewMatrix = mainCamera->GetViewMatrix();
        glm::mat4 projectionMatrix = mainCamera->GetProjectionMatrix();
        glm::mat4 vp = projectionMatrix * viewMatrix;
        glm::vec3 viewPos = camTsm->GetPosition();
        globalShaderResoruce.v.projection = projectionMatrix;
        globalShaderResoruce.v.viewProjection = vp;
        globalShaderResoruce.v.viewPos = viewPos;
        globalShaderResoruce.v.view = viewMatrix;
    }

    PrepareFrameData(cmdBuf);
    ProcessLights(gameScene, cmdBuf);

    // prepare
    std::vector<Gfx::ClearValue> clears(2);
    clears[0].color = {{0, 0, 0, 0}};
    clears[1].depthStencil.depth = 1;
    clears[1].depthStencil.stencil = 0;
    Rect2D scissor = {{0, 0}, {gfxDriver->GetWindowSize().width, gfxDriver->GetWindowSize().height}};
    cmdBuf->SetScissor(0, 1, &scissor);
    globalShaderResoruce.BindShaderResource(cmdBuf);

    // render scene object (opauqe + transparent)
    cmdBuf->BeginRenderPass(renderPass, clears);
    if (gameScene)
    {
        auto& roots = gameScene->GetRootObjects();
        for (auto root : roots)
        {
            RenderObject(root->GetTransform(), cmdBuf);
        }
    }
    cmdBuf->EndRenderPass();

#if GAME_EDITOR
    if (renderEditor)
    {
        gameEditor->GameRenderOutput(colorImage, depthImage);
        gameEditor->Render(cmdBuf);
    }
#else
    cmdBuf->Blit(colorImage, gfxDriver->GetSwapChainImageProxy());
#endif

    gfxDriver->QueueSubmit(mainQueue, {cmdBuf}, {imageAcquireSemaphore}, {PipelineStage::Color_Attachment_Output},
                           {mainCmdBufFinishedSemaphore}, mainCmdBufFinishedFence);
    gfxDriver->Present({mainCmdBufFinishedSemaphore});
}

void RenderPipeline::RenderObject(RefPtr<Transform> transform, UniPtr<CommandBuffer>& cmd)
{
    for (auto child : transform->GetChildren())
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
            // material->SetMatrix("Transform", "model",
            // meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
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

void RenderPipeline::ProcessLights(RefPtr<GameScene> gameScene, RefPtr<CommandBuffer> cmdBuf)
{
    auto lights = gameScene->GetActiveLights();
}

void RenderPipeline::ApplyAsset()
{
    asset = MakeUnique<RenderPipelineAsset>();
    asset->virtualTexture = userConfigAsset->virtualTexture;
}

void RenderPipeline::PrepareFrameData(RefPtr<CommandBuffer> cmdBuf)
{
    Internal::GetGfxResourceTransfer()->QueueTransferCommands(cmdBuf);
    globalShaderResoruce.QueueCommand(cmdBuf);

    // resource prepare | draw commands
    GPUBarrier barrier;
    barrier.srcStageMask = PipelineStage::Transfer;
    barrier.dstStageMask = PipelineStage::Vertex_Input | PipelineStage::Transfer;
    barrier.srcAccessMask = AccessMask::Transfer_Write;
    barrier.dstAccessMask = AccessMask::Memory_Read;

    cmdBuf->Barrier(&barrier, 1);
}

void RenderPipeline::GlobalShaderResource::QueueCommand(RefPtr<CommandBuffer> cmdBuf) {}

} // namespace Engine::Rendering
