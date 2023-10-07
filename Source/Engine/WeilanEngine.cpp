#include "WeilanEngine.hpp"
#if ENGINE_EDITOR
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include "ThirdParty/imgui/ImGuizmo.h"
#endif
namespace Engine
{
WeilanEngine::~WeilanEngine()
{
    gfxDriver->WaitForIdle();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void WeilanEngine::Init(const CreateInfo& createInfo)
{
    projectPath = createInfo.projectPath;

    Gfx::GfxDriver::CreateInfo gfxCreateInfo{{1240, 860}};
    gfxDriver = Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, gfxCreateInfo);
    assetDatabase = std::make_unique<AssetDatabase>(projectPath);
    renderPipeline = std::make_unique<RenderPipeline>();
    event = std::make_unique<Event>();
    frameCmdBuffer = std::make_unique<FrameCmdBuffer>(*gfxDriver);

    sceneRenderer = std::make_unique<Engine::SceneRenderer>();
    sceneRenderer->BuildGraph({
        .finalImage = *GetGfxDriver()->GetSwapChainImage(),
        .layout = Gfx::ImageLayout::Present_Src_Khr,
        .accessFlags = Gfx::AccessMask::None,
        .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
    }

    );
    sceneRenderer->Process();
    renderPipeline->RegisterSwapchainRecreateCallback(
        [this]
        {
            sceneRenderer->BuildGraph({
                .finalImage = *GetGfxDriver()->GetSwapChainImage(),
                .layout = Gfx::ImageLayout::Present_Src_Khr,
                .accessFlags = Gfx::AccessMask::None,
                .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
            });
            sceneRenderer->Process();
        }
    );

#if ENGINE_EDITOR
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(GetGfxDriver()->GetSDLWindow());
#endif
}

void WeilanEngine::BeginFrame()
{
    Time::Tick();

#if ENGINE_EDITOR
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
#endif

    // poll events, this is every important
    // the events are polled by SDL, somehow to show up the the window, we need it to poll the events!
    event->Poll();

    renderPipeline->AcquireSwapchainImage();
    frameCmdBuffer->GetActive()->Begin();
}

void WeilanEngine::EndFrame()
{
#if ENGINE_EDITOR
    ImGui::EndFrame();
#endif
    frameCmdBuffer->GetActive()->End();

    // submit anything in the active command and present the surface
    Rendering::CmdSubmitGroup cmdSubmitGroup;
    cmdSubmitGroup.AddCmd(frameCmdBuffer->GetActive());
    renderPipeline->Render(cmdSubmitGroup);

    // the active command buffer has submitted to GPU, we can swap to another command buffer
    frameCmdBuffer->Swap();
}

WeilanEngine::FrameCmdBuffer::FrameCmdBuffer(Gfx::GfxDriver& gfxDriver)
{
    auto queueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();
    cmdPool = gfxDriver.CreateCommandPool({queueFamilyIndex});

    cmd0 = cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];
    cmd1 = cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];

    activeCmd = cmd0.get();
}

void WeilanEngine::FrameCmdBuffer::Swap()
{
    if (activeCmd == cmd0.get())
    {
        activeCmd = cmd1.get();
    }
    else
    {
        activeCmd = cmd0.get();
    }

    // the newly actived will be reset here
    activeCmd->Reset(false);
}

Gfx::CommandBuffer& WeilanEngine::GetActiveCmdBuffer()
{
    return *frameCmdBuffer->GetActive();
}
} // namespace Engine
