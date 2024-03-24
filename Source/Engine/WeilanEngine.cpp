#include "WeilanEngine.hpp"
#include "Core/GameLoop.hpp"
#if ENGINE_EDITOR
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include "Physics/Physics.hpp"
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
// clang-format on
WeilanEngine::WeilanEngine(){};

WeilanEngine::~WeilanEngine()
{
    gfxDriver->WaitForIdle();
    DeinitJoltPhysics();
    // physics->Destroy();
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
    InitJoltPhysics();
    assetDatabase = std::make_unique<AssetDatabase>(projectPath);
    AssetDatabase::SingletonReference() = assetDatabase.get();
    // physics = std::make_unique<Physics>();
    // physics->Init();
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
    std::unique_ptr<GameLoop> loop = std::make_unique<GameLoop>();
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

static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    // Print to the TTY
    spdlog::info(buffer);
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
    spdlog::error("{}: {}:({}) ({})", inFile, inLine, inExpression, inMessage == nullptr ? inMessage : "");

    // Breakpoint
    return true;
};

#endif // JPH_ENABLE_ASSERTS

void WeilanEngine::InitJoltPhysics()
{
    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if
    // you want (see Memory.h). This needs to be done before any other Jolt function is called.
    JPH::RegisterDefaultAllocator();

    // Install trace and assert callbacks
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    // Create a factory, this class is responsible for creating instances of classes based on their name or hash and
    // is mainly used for deserialization of saved data. It is not directly used in this example but still required.
    JPH::Factory::sInstance = new JPH::Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch
    // class. If you have your own custom shape types you probably need to register their handlers with the
    // CollisionDispatch before calling this function. If you implement your own default material
    // (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create
    // one for you.
    JPH::RegisterTypes();
}

void WeilanEngine::DeinitJoltPhysics()
{
    // Unregisters all types with the factory and cleans up the default material
    JPH::UnregisterTypes();

    // Destroy the factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
