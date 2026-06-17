#include "WeilanEngine.hpp"
#include "Editor/GameEditor.hpp"
#include "Engine/Core/DelayDestroy.hpp"
#include "Engine/Core/GameLoop.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#if defined(_WIN32) || defined(_WIN64)
#include "Engine/Driver/WindowSystemHost/D3D11/D3D11InteropDriverCreateHelper.hpp"
#endif
#include "Engine/MiddleLayer/FrameContext.hpp"
#include "Engine/MiddleLayer/PlatformSpecific/TransparentWindowPixel.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/MaterialUploadManager.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBindings.hpp"
#if ENGINE_EDITOR
#include "Engine/ThirdParty/imgui/ImGuizmo.h"
#include "Engine/ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
// clang-format on
//
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#ifdef WEILAN_ENABLE_MCP
#include "Engine/Runtime/MCP/MCPServer.hpp"
#endif
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
WeilanEngine::WeilanEngine() {};

WeilanEngine::~WeilanEngine()
{
#ifdef WEILAN_ENABLE_MCP
    mcpServer = nullptr;
#endif
    editor = nullptr;
    event->Deinit();
    UI::Instance().Destroy();
    gfxDriver->WaitForIdle();
    DelayDestroy::Singleton()->Flush();
    ClearLuaCreatedRuntimeAssets();
    gameLoop = nullptr;
    DeinitAssetDatabase();
    ShaderLibrary::Singleton().DestoryShaderLibrary();
    DeinitJoltPhysics();
    Rendering::GPUDrivenManager::Instance().Deinit();
    gfxDriver = nullptr;
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    DeinitSDL();

    JobSystem::DeinitJobSystem();
}

void WeilanEngine::Init(const CreateInfo& createInfo)
{
    JobSystem::InitJobSystem();
    InitSDL();
    projectPath = createInfo.projectPath;
    projectAssetPath = createInfo.projectPath / "Assets";
    gameContext = std::make_unique<GameContext>();

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

    std::filesystem::path engineConfigPath =
        std::filesystem::path(ENGINE_SOURCE_PATH) / "Resources/DefaultEngineConfig.json";
    std::ifstream engineConfigFile(engineConfigPath);
    nlohmann::json engineConfig = nlohmann::json::parse(engineConfigFile);
    const nlohmann::json& gfxDriverConfig = engineConfig.value("gfxDriver", nlohmann::json::object());
    Gfx::GfxDriver::CreateInfo gfxCreateInfo{
        .window = mainWindow.handle,
        .enableRenderDoc = gfxDriverConfig.value("enableRenderDoc", false),
        .enableGfxDriverValidation = gfxDriverConfig.value("enableValidationLayer", false),
        .enableGPUTimestamp = gfxDriverConfig.value("enableGPUTimestamp", false),
        .gpuTimestampQueryMaxCount = gfxDriverConfig.value("gpuTimestampQueryMaxCount", 1024)
    };
    gfxDriver = Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, gfxCreateInfo);

    InitAssetDatabase();
    InitJoltPhysics();
    event = std::make_unique<Event>();
    event->Init();
#if ENGINE_EDITOR
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(GetGfxDriver()->GetSDLWindow());
#endif

    luaBackend->Init(assetDatabase->GetAssetDirectory().string().c_str());
    gameLoop = std::make_unique<GameLoop>();

    ShaderLibrary::Singleton().WaitForShaderCompilation();

#ifdef WEILAN_ENABLE_MCP
    if (createInfo.enableMCP)
    {
        mcpServer = std::make_unique<MCPServer>(8080);
        mcpServer->Start();
    }
#endif

    editor = std::make_unique<Editor::GameEditor>(this, createInfo.projectPath.string().c_str());
    cmd = GetGfxDriver()->CreateCommandBuffer();

    blitShader = ShaderLibrary::GetShader("Blit", {"_Premultiplied"});

    // TransparentWindowPixel::ForceTopmost(mainWindow.handle);
}

void WeilanEngine::StartEngine()
{
    while (keepLooping)
    {
        if (BeginFrame())
        {
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            // update gameloop
            auto screenSize = editor->GetGameScreenSize();
            const Gfx::ImageIdentifier* gameOutputImage = nullptr;
            const Gfx::ImageIdentifier* gameOutputDepthImage = nullptr;
            bool offscreen = !editor->IsGameViewVisible();

            editor->Tick();
#ifdef WEILAN_ENABLE_MCP
            if (mcpServer)
                mcpServer->Tick();
#endif
            gameLoop->Tick(screenSize, gameOutputImage, gameOutputDepthImage, offscreen);
            editor->AfterGameLoopTick();

            if (presentGameColorOnly)
            {
                auto swapchainImage = GetGfxDriver()->GetSwapChainImage();
                std::vector<Gfx::DynamicBinding> bindings{
                    Gfx::DynamicBinding("input", *gameOutputImage)
                };
                Gfx::RenderAttachment attachments[] = {
                    Gfx::RenderAttachment(
                        *swapchainImage,
                        Gfx::AttachmentLoadOperation::Clear,
                        Gfx::AttachmentStoreOperation::Store
                    )
                };
                Gfx::ClearValue clears[] = {
                    {0.0f, 0.0f, 0.0f, 0.0f},
                };

                cmd->BeginRenderPass(attachments, clears);
                cmd->BindShaderProgram(blitShader->GetShaderProgram(), blitShader->GetShaderProgram()->GetDefaultShaderConfig());
                cmd->BindResource(blitShader->GetSet("perMaterial"), bindings);
                cmd->Draw(6, 1, 0, 0);
                cmd->EndRenderPass();
            }
            else
            {
                editor->Render(*cmd, gameOutputImage, gameOutputDepthImage);
            }

            GetGfxDriver()->ExecuteCommandBuffer(*cmd);
            cmd->Reset(true);

            EndFrame();

            if (presentGameColorOnly)
            {
                GetGfxDriver()->WaitForIdle();
                interopDriver->Present();
            }
        }
    }
}

bool WeilanEngine::BeginFrame()
{
    ENGINE_BEGIN_FRAME_PROFILE

    ENGINE_BEGIN_PROFILE("Begin Frame");

    ENGINE_BEGIN_PROFILE("Frame Cap");
    const float frameCap = 1.0f / 60.0f;
    float delta = Time::RealtimedDeltaTime();
    if (delta < frameCap)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds((int)((frameCap - delta) * 1000)));
    }
    ENGINE_END_PROFILE; // Frame Cap

    ENGINE_BEGIN_PROFILE("Prepare Frame");
    Time::Tick();
    GetFrameContext().BeginFrame();

    Input::Reset();
    // poll events, this is every important
    // the events are polled by SDL, somehow to show up the the window, we need it to poll the events!
    event->Poll();
    ENGINE_END_PROFILE; // Prepare Frame

    ENGINE_BEGIN_PROFILE("GfxDriver BeginFrame");
    bool shouldBeginFrame = gfxDriver->BeginFrame();
    ENGINE_END_PROFILE; // GfxDriver BeginFrame

    ENGINE_END_PROFILE; // Begin Frame

    return shouldBeginFrame;
}

void WeilanEngine::EndFrame()
{
    ENGINE_BEGIN_PROFILE("End Frame");
    event->Reset();

    assetDatabase->PollAsyncLoadingResults();

    MaterialUploadManager::Instance().FlushPendingUploads();

    // submit anything in the active command and present the surface
    if (gfxDriver->EndFrame())
        event->swapchainRecreated.state = true;
    else
        event->swapchainRecreated.state = false;

    Graphics::GetSingleton().ClearDraws();
    DelayDestroy::Singleton()->Flush();
    GetFrameContext().EndFrame();
#if ENGINE_EDITOR
    if (assetDatabase->RefreshShader())
    {
        gfxDriver->ShaderReloaded();
    }
#endif

    ENGINE_END_PROFILE; // End Frame

    ENGINE_END_FRAME_PROFILE
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

    JoltDebugRenderer::Init();
}

void WeilanEngine::DeinitJoltPhysics()
{
    JoltDebugRenderer::Deinit();

    // Unregisters all types with the factory and cleans up the default material
    JPH::UnregisterTypes();

    // Destroy the factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void WeilanEngine::InitSDL()
{
    SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

    SDL_DisplayMode displayMode;
    // MacOS return points not pixels
    SDL_GetCurrentDisplayMode(0, &displayMode);

    if (mainWindow.size.width > displayMode.w)
        mainWindow.size.width = displayMode.w;
    if (mainWindow.size.height > displayMode.h)
        mainWindow.size.height = displayMode.h;

    mainWindow.handle = SDL_CreateWindow(
        "WeilanGame",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        mainWindow.size.width,
        mainWindow.size.height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    SDL_MaximizeWindow(mainWindow.handle);

    int drawableWidth, drawbaleHeight;
    SDL_GL_GetDrawableSize(mainWindow.handle, &drawableWidth, &drawbaleHeight);

    mainWindow.size.width = drawableWidth;
    mainWindow.size.height = drawbaleHeight;
}

void WeilanEngine::WindowBorderless(bool enable)
{
    SDL_SetWindowBordered(mainWindow.handle, enable ? SDL_FALSE : SDL_TRUE);
}

void WeilanEngine::DeinitAssetDatabase()
{
    assetDatabase = nullptr;
}

void WeilanEngine::InitAssetDatabase()
{
    assetDatabase = std::make_unique<AssetDatabase>();
    AssetDatabase::SingletonReference() = assetDatabase.get();
    assetDatabase->Init(projectPath);
}

void WeilanEngine::DeinitSDL()
{
    // destroy appWindow
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void WeilanEngine::ReloadScripts()
{
    assetDatabase->ReloadScripts();
}

void WeilanEngine::CloseEngine()
{
    keepLooping = false;
}

void WeilanEngine::SetSystemWindowSize(int2 size)
{
    SDL_SetWindowSize(mainWindow.handle, size.x, size.y);
    mainWindow.size.width = size.x;
    mainWindow.size.height = size.y;
}

int2 WeilanEngine::GetSystemWindowSize()
{
    int w, h;
    SDL_GetWindowSize(mainWindow.handle, &w, &h);
    return int2{w, h};
}

void WeilanEngine::PresentGameOnly(bool enable)
{
#if defined(_WIN32) || defined(_WIN64)
    presentGameColorOnly = enable;

    // lazy create interop driver
    if (window_HWND == nullptr)
    {
        window_HWND = WeilanEngine_CreateWindow();
        interopDriver = CreateD3D11InteropDriver();
        interopDriver->Initialize(window_HWND, mainWindow.size.width, mainWindow.size.height);
    }

    if (presentGameColorOnly)
    {
        auto intermediateTextureHandle = interopDriver->GetSharedHandle();
        gfxDriver->SetWin32WindowInteropTexture(intermediateTextureHandle, int2(mainWindow.size.width, mainWindow.size.height));
    }
    else
    {
        gfxDriver->UnsetWin32WindowInteropTexture(int2(mainWindow.size.width, mainWindow.size.height));
    }
#else
    if (enable)
    {
        SPDLOG_WARN("PresentGameOnly is only supported on Windows.");
    }
    presentGameColorOnly = false;
#endif
}
