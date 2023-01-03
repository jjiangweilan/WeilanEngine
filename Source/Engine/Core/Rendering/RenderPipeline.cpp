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

void RenderPipeline::Init(RefPtr<Editor::GameEditorRenderer> gameEditorRenderer)
{
    if (gameEditorRenderer != nullptr)
    {
        renderEditor = true;
        this->gameEditorRenderer = gameEditorRenderer;
    }

    // create color and depth
    ImageDescription imageDescription{};
    imageDescription.format = ImageFormat::R16G16B16A16_SFloat;
    imageDescription.width = gfxDriver->GetWindowSize().width;
    imageDescription.height = gfxDriver->GetWindowSize().height;
    imageDescription.multiSampling = MultiSampling::Sample_Count_1;
    imageDescription.mipLevels = 1;
    colorImage = Gfx::GfxDriver::Instance()->CreateImage(imageDescription,
                                                         ImageUsage::ColorAttachment | ImageUsage::TransferSrc |
                                                             ImageUsage::Texture);
    colorImage->SetName("Color Image");
    imageDescription.format = ImageFormat::D24_UNorm_S8_UInt;
    depthImage = Gfx::GfxDriver::Instance()->CreateImage(imageDescription,
                                                         ImageUsage::DepthStencilAttachment | ImageUsage::TransferSrc);
    depthImage->SetName("Depth Image");

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
    mainCmdBufFinishedSemaphore = GetGfxDriver()->CreateSemaphore({false});
    mainCmdBufFinishedFence = gfxDriver->CreateFence({true});
    cmdPool = gfxDriver->CreateCommandPool({mainQueue->GetFamilyIndex()});
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

    renderPassNode = graph.AddNode<RGraph::RenderPassNode>();
    renderPassNode->SetColorCount(1);
    renderPassNode->GetColorAttachmentOps()[0].loadOp = AttachmentLoadOperation::Clear;
    renderPassNode->GetColorAttachmentOps()[0].storeOp = AttachmentStoreOperation::Store;
    renderPassNode->GetDepthAttachmentOp().loadOp = AttachmentLoadOperation::Clear;
    renderPassNode->GetDepthAttachmentOp().storeOp = AttachmentStoreOperation::Store;
    Gfx::ClearValue colorClear;
    colorClear.color = {{0, 0, 0, 0}};
    renderPassNode->GetClearValues().push_back(colorClear);
    Gfx::ClearValue depthClear;
    depthClear.depthStencil.depth = 1;
    depthClear.depthStencil.stencil = 0;
    renderPassNode->GetClearValues().push_back(depthClear);

    colorImageNode->GetPortOutput()->Connect(renderPassNode->GetPortColorIn(0));
    depthImageNode->GetPortOutput()->Connect(renderPassNode->GetPortDepthIn());

    Port* finalOutput = renderPassNode->GetPortColorOut(0);
#if GAME_EDITOR
    finalOutput =
        gameEditorRenderer->BuildGraph(&graph, renderPassNode->GetPortColorOut(0), renderPassNode->GetPortDepthOut());
#endif

    swapChainImageNode = graph.AddNode<RGraph::ImageNode>();
    swapChainImageNode->SetExternalImage(GetGfxDriver()->GetSwapChainImageProxy().Get());
    swapChainImageNode->initialState = {Gfx::PipelineStage::Bottom_Of_Pipe,
                                        Gfx::AccessMask::None,
                                        Gfx::ImageLayout::Undefined,
                                        GFX_QUEUE_FAMILY_IGNORED,
                                        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst};

    blitNode = graph.AddNode<RGraph::BlitNode>();
    blitNode->SetName("RenderPipeline-Blit-FinalOutputToSwapChain");
    blitNode->GetPortSrcImageIn()->Connect(finalOutput);
    blitNode->GetPortDstImageIn()->Connect(swapChainImageNode->GetPortOutput());

    swapChainToPesentNode = graph.AddNode<RGraph::GPUBarrierNode>();
    swapChainToPesentNode->GetPortImageIn()->Connect(blitNode->GetPortDstImageOut());
    swapChainToPesentNode->layout = Gfx::ImageLayout::Present_Src_Khr;
    swapChainToPesentNode->stageFlags = Gfx::PipelineStage::Top_Of_Pipe;
    swapChainToPesentNode->accessFlags = Gfx::AccessMask::None;

    graph.Compile();
}

void RenderPipeline::Render(RefPtr<GameScene> gameScene)
{
    gfxDriver->AcquireNextSwapChainImage(imageAcquireSemaphore);
    gfxDriver->WaitForFence({mainCmdBufFinishedFence}, true, -1);
    mainCmdBufFinishedFence->Reset();
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
        Internal::GetGfxResourceTransfer()->Transfer(globalShaderResoruce.GetShaderResource().Get(),
                                                     "SceneInfo",
                                                     "viewProjection",
                                                     &globalShaderResoruce.v.viewProjection);
        Internal::GetGfxResourceTransfer()->Transfer(globalShaderResoruce.GetShaderResource().Get(),
                                                     "SceneInfo",
                                                     "projection",
                                                     &globalShaderResoruce.v.projection);
        Internal::GetGfxResourceTransfer()->Transfer(globalShaderResoruce.GetShaderResource().Get(),
                                                     "SceneInfo",
                                                     "viewPos",
                                                     &globalShaderResoruce.v.viewPos);
        Internal::GetGfxResourceTransfer()->Transfer(globalShaderResoruce.GetShaderResource().Get(),
                                                     "SceneInfo",
                                                     "view",
                                                     &globalShaderResoruce.v.view);
    }

    cmdBuf->Begin();
    PrepareFrameData(cmdBuf);
    ProcessLights(gameScene, cmdBuf);

    // prepare
    auto& drawList = renderPassNode->GetDrawList();
    drawList.clear();

    Rect2D scissor = {{0, 0}, {gfxDriver->GetWindowSize().width, gfxDriver->GetWindowSize().height}};
    DrawData drawData;
    // cmdBuf->SetScissor(0, 1, &scissor);
    drawData.scissor = scissor;
    drawData.shaderResource = globalShaderResoruce.GetShaderResource().Get();
    drawList.push_back(drawData);

    // render scene object (opauqe + transparent)
    // cmdBuf->BeginRenderPass(renderPass, clears);
    if (gameScene)
    {
        auto& roots = gameScene->GetRootObjects();
        for (auto root : roots)
        {
            RenderObject(root->GetTransform(), cmdBuf, drawList);
        }
    }
    // cmdBuf->EndRenderPass();

#if GAME_EDITOR
    if (renderEditor)
    {
        gameEditorRenderer->Render();
    }
#else
    // cmdBuf->Blit(colorImage, gfxDriver->GetSwapChainImageProxy());
#endif

    RGraph::ResourceStateTrack stateTrack;
    graph.Execute(cmdBuf.Get(), stateTrack);
    cmdBuf->End();

    RefPtr<CommandBuffer> cmdBufs[] = {cmdBuf};
    RefPtr<Semaphore> semaphores[] = {imageAcquireSemaphore};
    Gfx::PipelineStageFlags pipelineStageFlags[] = {PipelineStage::Color_Attachment_Output};
    RefPtr<Semaphore> signalSemaphores[] = {mainCmdBufFinishedSemaphore};
    gfxDriver->QueueSubmit(mainQueue,
                           cmdBufs,
                           semaphores,
                           pipelineStageFlags,
                           signalSemaphores,
                           mainCmdBufFinishedFence); // Queue submit
    gfxDriver->Present({mainCmdBufFinishedSemaphore});

    // overide swapChainImageNode's initial state after the first frame
    swapChainImageNode->initialState = {Gfx::PipelineStage::Bottom_Of_Pipe,
                                        Gfx::AccessMask::None,
                                        Gfx::ImageLayout::Undefined,
                                        GFX_QUEUE_FAMILY_IGNORED,
                                        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst};
}

void RenderPipeline::RenderObject(RefPtr<Transform> transform,
                                  UniPtr<CommandBuffer>& cmd,
                                  std::vector<RGraph::DrawData>& drawList)
{
    for (auto child : transform->GetChildren())
    {
        RenderObject(child, cmd, drawList);
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
            auto& meshBindingInfo = mesh->GetMeshBindingInfo();
            DrawData drawData;

            drawData.vertexBuffer = std::vector<VertexBufferBinding>();
            for (int i = 0; i < meshBindingInfo.bindingBuffers.size(); ++i)
            {
                drawData.vertexBuffer = meshBindingInfo.bindingBuffers;
            }
            drawData.indexBuffer = RGraph::IndexBuffer{meshBindingInfo.indexBuffer.Get(),
                                                       meshBindingInfo.indexBufferOffset,
                                                       mesh->GetIndexBufferType()};

            drawData.shaderResource = material->GetShaderResource().Get();
            drawData.shader = shader->GetShaderProgram().Get();
            drawData.shaderConfig = &material->GetShaderConfig();
            auto modelMatrix = meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix();
            drawData.pushConstant =
                RGraph::PushConstant{material->GetShader()->GetShaderProgram().Get(), modelMatrix, glm::mat4(0)};
            drawData.drawIndexed = RGraph::DrawIndex{vertexInfo.index.count, 1, 0, 0, 0};
            drawList.push_back(drawData);
            // cmd->BindResource(material->GetShaderResource());
            // cmd->BindShaderProgram(shader->GetShaderProgram(), material->GetShaderConfig());
            // cmd->SetPushConstant(shader->GetShaderProgram(), &modelMatrix);
            // cmd->DrawIndexed(vertexInfo.index.count, 1, 0, 0, 0);
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
    barrier.dstStageMask = PipelineStage::All_Commands;
    barrier.srcAccessMask = AccessMask::Transfer_Write;
    barrier.dstAccessMask = AccessMask::Memory_Read | AccessMask::Memory_Write;

    cmdBuf->Barrier(&barrier, 1);
}

void RenderPipeline::GlobalShaderResource::QueueCommand(RefPtr<CommandBuffer> cmdBuf) {}

} // namespace Engine::Rendering
