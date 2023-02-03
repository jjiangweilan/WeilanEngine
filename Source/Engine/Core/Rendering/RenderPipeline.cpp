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

    mainQueue = gfxDriver->GetQueue(QueueType::Main);
    imageAcquireSemaphore = GetGfxDriver()->CreateSemaphore({false});
    imageAcquireSemaphore->SetName("Pipelineline-imageAcquireSemaphore");
    mainCmdBufFinishedSemaphore = GetGfxDriver()->CreateSemaphore({false});
    mainCmdBufFinishedSemaphore->SetName("Pipelineline-mainCmdBufFinishedSemaphore");
    resourceTransferFinishedSemaphore = GetGfxDriver()->CreateSemaphore({false});
    resourceTransferFinishedSemaphore->SetName("Pipelineline-resourceTransferFinishedSemaphore");
    mainCmdBufFinishedFence = gfxDriver->CreateFence({true});
    cmdPool = gfxDriver->CreateCommandPool({mainQueue->GetFamilyIndex()});
    auto cmdBufs = cmdPool->AllocateCommandBuffers(CommandBufferType::Primary, 2);
    cmdBuf = std::move(cmdBufs[0]);
    resourceTransferCmdBuf = std::move(cmdBufs[1]);

    // build render graph
    colorImageNode = graph.AddNode<RGraph::ImageNode>();
    colorImageNode->width = gfxDriver->GetSurfaceSize().width;
    colorImageNode->height = gfxDriver->GetSurfaceSize().height;
    colorImageNode->mipLevels = 1;
    colorImageNode->multiSampling = MultiSampling::Sample_Count_1;
    colorImageNode->format = ImageFormat::R16G16B16A16_SFloat;
    colorImageNode->SetName("RenderPipeline-colorImageNode");

    depthImageNode = graph.AddNode<RGraph::ImageNode>();
    depthImageNode->width = colorImageNode->width;
    depthImageNode->height = colorImageNode->height;
    depthImageNode->mipLevels = colorImageNode->mipLevels;
    depthImageNode->multiSampling = colorImageNode->multiSampling;
    depthImageNode->format = ImageFormat::D24_UNorm_S8_UInt;
    depthImageNode->SetName("RenderPipeline-depthImageNode");

    renderPassNode = graph.AddNode<RGraph::RenderPassNode>();
    renderPassNode->SetColorCount(1);
    renderPassNode->GetColorAttachmentOps()[0].loadOp = AttachmentLoadOperation::Clear;
    renderPassNode->GetColorAttachmentOps()[0].storeOp = AttachmentStoreOperation::Store;
    renderPassNode->GetDepthAttachmentOp().loadOp = AttachmentLoadOperation::Clear;
    renderPassNode->GetDepthAttachmentOp().storeOp = AttachmentStoreOperation::Store;
    renderPassNode->SetName("RenderPipeline-renderPassNode");
    Gfx::ClearValue colorClear;
    colorClear.color = {{0, 0, 0, 0}};
    renderPassNode->GetClearValues()[0] = colorClear;
    Gfx::ClearValue depthClear;
    depthClear.depthStencil.depth = 1;
    depthClear.depthStencil.stencil = 0;
    renderPassNode->GetClearValues().back() = depthClear;
    renderPassNode->SetDrawList(sceneDrawList);

    colorImageNode->GetPortOutput()->Connect(renderPassNode->GetPortColorIn(0));
    depthImageNode->GetPortOutput()->Connect(renderPassNode->GetPortDepthIn());

    Port* finalOutput = renderPassNode->GetPortColorOut(0);
#if GAME_EDITOR
    finalOutput = gameEditorRenderer->BuildGraph(&graph,
                                                 renderPassNode->GetPortColorOut(0),
                                                 renderPassNode->GetPortDepthOut(),
                                                 gfxDriver->GetSurfaceSize());
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

    vt = MakeUnique<VirtualTexture>("VT", 16384, 8192, 3);
    virtualTextureRenderer.SetVT(vt,
                                 GetGfxDriver()->GetSurfaceSize().width,
                                 GetGfxDriver()->GetSurfaceSize().height,
                                 8);
    virtualTextureRenderer.SetVTObjectDrawList(sceneDrawList);
    virtualTextureRenderer.GetFinalTextures(vtCacheTex, vtIndirTex);

    vtCacheTexStagingBuffer = graph.AddNode<RGraph::BufferNode>();
    vtCacheTexStagingBuffer->props.debugName = "RenderPipeline-vtCacheTexStagingBuffer";
    vtCacheTexStagingBuffer->props.size = vtCacheTex->GetSize();
    vtCacheTexStagingBuffer->props.visibleInCPU = true;

    vtIndirTexStagingBuffer = graph.AddNode<RGraph::BufferNode>();
    vtIndirTexStagingBuffer->props.debugName = "RenderPipeline-vtIndirTexStagingBuffer";
    vtIndirTexStagingBuffer->props.size = vtIndirTex->GetSize();
    vtIndirTexStagingBuffer->props.visibleInCPU = true;

    // TODO use the `if` node in render graph
    vtCacheTexNode = graph.AddNode<RGraph::ImageNode>();
    vtCacheTexNode->width = vtCacheTex->GetWidth();
    vtCacheTexNode->height = vtCacheTex->GetHeight();
    vtCacheTexNode->format = Gfx::ImageFormat::R8G8B8A8_SRGB;
    vtCacheTexNode->SetName("RenderPipeline-vtCacheTexNode");
    vtIndirTexNode = graph.AddNode<RGraph::ImageNode>();
    vtIndirTexNode->width = vtIndirTex->GetWidth();
    vtIndirTexNode->height = vtIndirTex->GetHeight();
    vtIndirTexNode->format = Gfx::ImageFormat::R32G32B32A32_SFloat;
    vtIndirTexNode->SetName("RenderPipeline-vtIndirTexNode");
    auto vtCacheTexTransferNode = graph.AddNode<RGraph::MemoryTransferNode>();
    vtCacheTexTransferNode->in.srcBuffer->Connect(vtCacheTexStagingBuffer->out.buffer);
    vtCacheTexTransferNode->in.dstImage->Connect(vtCacheTexNode->GetPortOutput());
    auto vtIndirTexTransferNode = graph.AddNode<RGraph::MemoryTransferNode>();
    vtIndirTexTransferNode->in.srcBuffer->Connect(vtIndirTexStagingBuffer->out.buffer);
    vtIndirTexTransferNode->in.dstImage->Connect(vtIndirTexNode->GetPortOutput());

    renderPassNode->GetPortDependentAttachmentsIn()->Connect(vtCacheTexNode->GetPortOutput());
    renderPassNode->GetPortDependentAttachmentsIn()->Connect(vtIndirTexNode->GetPortOutput());

    CompileGraph();
}

void RenderPipeline::CompileGraph()
{
    colorImageNode->width = gfxDriver->GetSurfaceSize().width;
    colorImageNode->height = gfxDriver->GetSurfaceSize().height;
    depthImageNode->width = colorImageNode->width;
    depthImageNode->height = colorImageNode->height;
#if GAME_EDITOR
    gameEditorRenderer->ResizeWindow(gfxDriver->GetSurfaceSize());
#endif
    graph.Compile();
    globalShaderResoruce.GetShaderResource()->SetTexture(
        "vtCache",
        (Gfx::Image*)vtCacheTexNode->GetPortOutput()->GetResourceVal());
    globalShaderResoruce.GetShaderResource()->SetTexture(
        "vtIndir",
        (Gfx::Image*)vtIndirTexNode->GetPortOutput()->GetResourceVal());
}

void RenderPipeline::Render(RefPtr<GameScene> gameScene)
{
    bool swapchainRecreated = gfxDriver->AcquireNextSwapChainImage(imageAcquireSemaphore);
    if (swapchainRecreated)
    {
        CompileGraph();
    }
    gfxDriver->WaitForFence({mainCmdBufFinishedFence}, true, -1);
    mainCmdBufFinishedFence->Reset();
    cmdPool->ResetCommandPool();

    Camera* mainCamera = Camera::mainCamera.Get();
    if (mainCamera != nullptr)
    {
        RefPtr<Transform> camTsm = mainCamera->GetGameObject()->GetTransform();

        ProcessLights(gameScene);

        glm::mat4 viewMatrix = mainCamera->GetViewMatrix();
        glm::mat4 projectionMatrix = mainCamera->GetProjectionMatrix();
        glm::mat4 vp = projectionMatrix * viewMatrix;
        glm::vec3 viewPos = camTsm->GetPosition();
        globalShaderResoruce.sceneInfo.projection = projectionMatrix;
        globalShaderResoruce.sceneInfo.viewProjection = vp;
        globalShaderResoruce.sceneInfo.viewPos = viewPos;
        globalShaderResoruce.sceneInfo.view = viewMatrix;

        Internal::GetGfxResourceTransfer()->Transfer(globalShaderResoruce.GetShaderResource().Get(),
                                                     "SceneInfo",
                                                     &globalShaderResoruce.sceneInfo);
    }

    // prepare
    sceneDrawList.clear();
    Rect2D scissor = {{0, 0}, {gfxDriver->GetSurfaceSize().width, gfxDriver->GetSurfaceSize().height}};
    DrawData drawData;
    // cmdBuf->SetScissor(0, 1, &scissor);
    drawData.scissor = scissor;
    drawData.shaderResource = globalShaderResoruce.GetShaderResource().Get();
    sceneDrawList.push_back(drawData);

    // render scene object (opauqe + transparent)
    // cmdBuf->BeginRenderPass(renderPass, clears);
    if (gameScene)
    {
        auto& roots = gameScene->GetRootObjects();
        for (auto root : roots)
        {
            AppendDrawData(root->GetTransform(), sceneDrawList);
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

    // transfer resources
    resourceTransferCmdBuf->Begin();
    PrepareFrameData(resourceTransferCmdBuf);
    resourceTransferCmdBuf->End();
    RefPtr<CommandBuffer> resTransCmdBufs[] = {resourceTransferCmdBuf};
    // virtualTextureRenderer.GetFeedbackGenerationBeginSemaphore();

    RefPtr<Semaphore> resTransSignalSemaphores[] = {resourceTransferFinishedSemaphore};
    gfxDriver->QueueSubmit(mainQueue, resTransCmdBufs, {}, {}, resTransSignalSemaphores,
                           nullptr); // Queue submit

    // vt
    if (false) // TODO: we need a feature toggle
    {
        virtualTextureRenderer.RunAnalysis();
        Gfx::Buffer* cacheTexBuf = (Gfx::Buffer*)vtCacheTexStagingBuffer->out.buffer->GetResourceVal();
        memcpy(cacheTexBuf->GetCPUVisibleAddress(), vtCacheTex->GetData(), vtCacheTex->GetSize());
        Gfx::Buffer* indirTexBuf = (Gfx::Buffer*)vtIndirTexStagingBuffer->out.buffer->GetResourceVal();
        memcpy(indirTexBuf->GetCPUVisibleAddress(), vtIndirTex->GetData(), vtIndirTex->GetSize());
    }

    cmdBuf->Begin();
    graph.Execute(cmdBuf.Get(), stateTrack);
    cmdBuf->End();

    RefPtr<CommandBuffer> cmdBufs[] = {cmdBuf};
    RefPtr<Semaphore> semaphores[] = {imageAcquireSemaphore, resourceTransferFinishedSemaphore};
    Gfx::PipelineStageFlags pipelineStageFlags[] = {PipelineStage::Color_Attachment_Output, PipelineStage::Transfer};
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

void RenderPipeline::AppendDrawData(RefPtr<Transform> transform, std::vector<RGraph::DrawData>& drawList)
{
    for (auto child : transform->GetChildren())
    {
        AppendDrawData(child, drawList);
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

void RenderPipeline::ProcessLights(RefPtr<GameScene> gameScene)
{
    auto lights = gameScene->GetActiveLights();

    for (int i = 0; i < lights.size(); ++i)
    {
        globalShaderResoruce.sceneInfo.lights[i].intensity = lights[i]->GetIntensity();
        auto model = lights[i]->GetGameObject()->GetTransform()->GetModelMatrix();
        globalShaderResoruce.sceneInfo.lights[i].position = glm::vec4(glm::normalize(glm::vec3(model[2])), 0);
    }
    globalShaderResoruce.sceneInfo.lightCount = lights.size();
}

void RenderPipeline::ApplyAsset()
{
    asset = MakeUnique<RenderPipelineAsset>();
    //  asset->virtualTexture = userConfigAsset->virtualTexture;
}

void RenderPipeline::PrepareFrameData(RefPtr<CommandBuffer> cmdBuf)
{
    Internal::GetGfxResourceTransfer()->QueueTransferCommands(cmdBuf);

    // resource prepare | draw commands
    GPUBarrier barrier;
    barrier.srcStageMask = PipelineStage::Transfer;
    barrier.dstStageMask = PipelineStage::All_Commands;
    barrier.srcAccessMask = AccessMask::Transfer_Write;
    barrier.dstAccessMask = AccessMask::Memory_Read | AccessMask::Memory_Write;

    cmdBuf->Barrier(&barrier, 1);
}

} // namespace Engine::Rendering
