#include "RenderPipeline.hpp"
#include "Core/Scene/Scene.hpp"

namespace Engine
{

RenderPipeline::RenderPipeline(SceneManager& sceneManager)
{
    submitFence = GetGfxDriver()->CreateFence({.signaled = true});
    submitSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
    commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
    cmd = commandPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];

#if ENGINE_EDITOR
    gameEditorRenderer = std::make_unique<Editor::Renderer>();
#endif
    sceneGraph = std::make_unique<Engine::DualMoonRenderer>(sceneManager);
    /// auto [swapchainNode, swapchainHandle, depthNode, depthHandle] = renderer->GetFinalSwapchainOutputs();
    // ProcessGraph(swapchainNode, swapchainHandle, depthNode, depthHandle);

    BuildAndProcess();

    RegisterSwapchainRecreateCallback([this, &sceneManager]() { BuildAndProcess(); });
}

void RenderPipeline::BuildAndProcess()
{
    sceneGraph->BuildGraph();
    auto [colorNode, colorHandle, depthNode, depthHandle] = sceneGraph->GetFinalSwapchainOutputs();
    ((RenderGraph::Graph*)sceneGraph.get())->Process(colorNode, colorHandle);
#if ENGINE_EDITOR
    auto [editorRenderNode, editorRenderNodeOutputHandle] = gameEditorRenderer->BuildGraph();
    gameEditorRenderer->Process(editorRenderNode, editorRenderNodeOutputHandle);
#endif
}

// void RenderPipeline::ProcessGraph(
//     RenderGraph::RenderNode* swapchainOutputNode,
//     RenderGraph::ResourceHandle swapchainOutputHandle,
//     RenderGraph::RenderNode* depthOutputNode,
//     RenderGraph::ResourceHandle depthOutputHandle
// )
// {
// #if ENGINE_EDITOR
//     auto [editorRenderNode, editorRenderNodeOutputHandle] = gameEditorRenderer->BuildGraph();
//     ((RenderGraph::Graph*)(this->renderer.get()))->Process(editorRenderNode, editorRenderNodeOutputHandle);
// #else
//     this->graph->Process(swapchainOutputNode, swapchainOutputHandle);
// #endif
// }

void RenderPipeline::Render()
{
    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();
    commandPool->ResetCommandPool();

    if (GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore))
    {
        swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
        if (GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore))
        {
            throw std::runtime_error("the second acquire swapchain image is not successful.");
        }

        for (auto& cb : swapchainRecreateCallback)
        {
            cb();
        }
    }

    // TODO: better move the whole gfx thing to a renderer
    float width = GetGfxDriver()->GetSwapChainImageProxy()->GetDescription().width;
    float height = GetGfxDriver()->GetSwapChainImageProxy()->GetDescription().height;
    cmd->Begin();
    cmd->SetViewport({.x = 0, .y = 0, .width = width, .height = height, .minDepth = 0, .maxDepth = 1});
    Rect2D rect = {{0, 0}, {(uint32_t)width, (uint32_t)height}};
    cmd->SetScissor(0, 1, &rect);

    if (sceneGraph)
        sceneGraph->Execute(*cmd);
#if ENGINE_EDITOR
    Gfx::GPUBarrier barrier{
        .buffer = nullptr,
        .image = nullptr,
        .srcStageMask = Gfx::PipelineStage::All_Commands,
        .dstStageMask = Gfx::PipelineStage::All_Commands,
        .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
    };
    cmd->Barrier(&barrier, 1);
    gameEditorRenderer->Execute(*cmd);
#endif
    cmd->End();

    RefPtr<Gfx::CommandBuffer> cmds[] = {cmd};
    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmds, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);

    GetGfxDriver()->Present({submitSemaphore});
}
} // namespace Engine
