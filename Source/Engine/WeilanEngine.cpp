#include "WeilanEngine.hpp"
#if ENGINE_EDITOR
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
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
    event = std::make_unique<Event>();
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

    if (!gfxDriver->BeginFrame())
        return false;

#if ENGINE_EDITOR
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
#endif

    return true;
}

void WeilanEngine::EndFrame()
{
#if ENGINE_EDITOR
    ImGui::EndFrame();
#endif

    event->Reset();

    // submit anything in the active command and present the surface
    if (gfxDriver->EndFrame())
        event->swapchainRecreated.state = true;
    else
        event->swapchainRecreated.state = false;

#if ENGINE_EDITOR
    assetDatabase->RefreshShader();
#endif
}

GameLoop* WeilanEngine::CreateGameLoop()
{
    auto loop = std::make_unique<GameLoop>();
    auto temp = loop.get();
    gameLoops.push_back(std::move(loop));
    return temp;
}

void WeilanEngine::DestroyGameLoop(GameLoop* loop)
{
    auto iter = std::find_if(gameLoops.begin(), gameLoops.end(), [loop](auto& l) { return l.get() == loop; });
    std::swap(*iter, gameLoops.back());
    gameLoops.pop_back();
}
