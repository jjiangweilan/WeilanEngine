#include "WeilanEngine.hpp"
#if ENGINE_EDITOR
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
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
    try
    {
        ringBufferLoggerSink = std::make_shared<spdlog::sinks::ringbuffer_sink<std::mutex>>(1024);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>(
            "engine logger",
            spdlog::sinks_init_list{ringBufferLoggerSink, consoleSink}
        );
        spdlog::set_default_logger(logger);
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }

    Gfx::GfxDriver::CreateInfo gfxCreateInfo{{1960, 1024}};
    gfxDriver = Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, gfxCreateInfo);
    assetDatabase = std::make_unique<AssetDatabase>(projectPath);
    renderPipeline = std::make_unique<RenderPipeline>();
    event = std::make_unique<Event>();
    frameCmdBuffer = std::make_unique<FrameCmdBuffer>(*gfxDriver);
#if ENGINE_EDITOR
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(GetGfxDriver()->GetSDLWindow());
#endif
}

bool WeilanEngine::BeginFrame()
{
    Time::Tick();
    // poll events, this is every important
    // the events are polled by SDL, somehow to show up the the window, we need it to poll the events!
    event->Poll();

    bool hasSwapchain = renderPipeline->AcquireSwapchainImage();
    if (!hasSwapchain)
    {
        return false;
    }

#if ENGINE_EDITOR
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
#endif
    frameCmdBuffer->GetActive()->Begin();

    return true;
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

#if ENGINE_EDITOR
    assetDatabase->RefreshShader();
#endif
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
