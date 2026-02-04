#pragma once
#include "Editor/IGameEditor.hpp"
#include "Engine/Core/GameLoop.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/AssetDatabase/Importers.hpp"
#include "Engine/Runtime/System/Event/Event.hpp"
#include "Engine/Runtime/System/SceneManager/SceneManager.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBackend.hpp"
#include <filesystem>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

namespace Editor
{
class GameEditor;
}
// class Physics;
class WeilanEngine
{
public:
    WeilanEngine();
    ~WeilanEngine();

public:
    struct CreateInfo
    {
        std::filesystem::path projectPath;
    };

    void Init(const CreateInfo& createInfo);

    bool BeginFrame();
    void EndFrame();
    void StartEngine();
    void CloseEngine();
    GameLoop* GetGameLoop() { return gameLoop.get(); };
    SDL_Window* GetMainWindow() { return mainWindow.handle; }

    std::shared_ptr<spdlog::sinks::ringbuffer_sink<std::mutex>> GetRingBufferLoggerSink()
    {
        return ringBufferLoggerSink;
    };

    const std::filesystem::path& GetProjectPath() { return projectPath; }

    const std::filesystem::path& GetProjectAssetPath() { return projectAssetPath; }

    void ReloadScripts();

    std::vector<std::function<void(SDL_Event& event)>> eventCallback;
    std::unique_ptr<Event> event;
    std::unique_ptr<Gfx::GfxDriver> gfxDriver;
    std::unique_ptr<AssetDatabase> assetDatabase;
    std::unique_ptr<LuaBackend> luaBackend;

private:
    struct MainWindow
    {
        SDL_Window* handle;
        Extent2D size = {1920, 1080};
    } mainWindow;

    bool keepLooping = true;
    std::shared_ptr<spdlog::sinks::ringbuffer_sink<std::mutex>> ringBufferLoggerSink;
    std::unique_ptr<GameLoop> gameLoop;
    std::unique_ptr<GameContext> gameContext;
    std::unique_ptr<Editor::GameEditor> editor;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    // std::unique_ptr<Physics> physics;

    std::filesystem::path projectPath;
    std::filesystem::path projectAssetPath;

    void InitAssetDatabase();
    void DeinitAssetDatabase();
    void InitJoltPhysics();
    void DeinitJoltPhysics();

    void InitSDL();
    void DeinitSDL();
};
