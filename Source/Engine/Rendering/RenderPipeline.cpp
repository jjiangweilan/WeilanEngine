#include "RenderPipeline.hpp"
#include "Core/Scene/Scene.hpp"
#include "DualMoonGraph/DualMoonGraph.hpp"

namespace Engine
{

RenderPipeline::RenderPipeline(Scene* scene) : scene(scene)
{
    submitFence = GetGfxDriver()->CreateFence({.signaled = true});
    submitSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
    commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
    cmd = commandPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];

    auto dualMoonGraph = std::make_unique<DualMoonGraph>(*scene);
    auto [color, colorHandle, depth, depthHandle] = dualMoonGraph->GetFinalSwapchainOutputs();
    graph = std::move(dualMoonGraph);

#if ENGINE_EDITOR
    gameEditorRenderer = std::make_unique<Editor::Renderer>();
    auto [editorRenderNode, editorRenderNodeOutputHandle] =
        gameEditorRenderer->BuildGraph(*graph, color, colorHandle, depth, depthHandle);
    this->graph->Process(editorRenderNode, editorRenderNodeOutputHandle);
#else
    this->graph->Process(color, colorHandle);
#endif
}

void RenderPipeline::Render()
{
    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();
    commandPool->ResetCommandPool();

    GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore);

    // TODO: better move the whole gfx thing to a renderer
    float width = GetGfxDriver()->GetSwapChainImageProxy()->GetDescription().width;
    float height = GetGfxDriver()->GetSwapChainImageProxy()->GetDescription().height;
    cmd->Begin();
    cmd->SetViewport({.x = 0, .y = 0, .width = width, .height = height, .minDepth = 0, .maxDepth = 1});
    Rect2D rect = {{0, 0}, {(uint32_t)width, (uint32_t)height}};
    cmd->SetScissor(0, 1, &rect);

    graph->Execute(*cmd);
    cmd->End();

    RefPtr<Gfx::CommandBuffer> cmds[] = {cmd};
    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmds, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);

    GetGfxDriver()->Present({submitSemaphore});
}
} // namespace Engine
