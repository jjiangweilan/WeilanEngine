#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Importers.hpp"
#include "Core/GameLoop.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Core/Time.hpp"
#include "Event/Event.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shaders.hpp"
#include <filesystem>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

class Physics;
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
    GameLoop* CreateGameLoop();
    void DestroyGameLoop(GameLoop* loop);
    void EndFrame();

    std::shared_ptr<spdlog::sinks::ringbuffer_sink<std::mutex>> GetRingBufferLoggerSink()
    {
        return ringBufferLoggerSink;
    };

    const std::filesystem::path& GetProjectPath()
    {
        return projectPath;
    }

    const std::filesystem::path& GetProjectAssetPath()
    {
        return projectPath;
    }

    std::vector<std::function<void(SDL_Event& event)>> eventCallback;
    std::unique_ptr<Event> event;
    std::unique_ptr<Gfx::GfxDriver> gfxDriver;
    std::unique_ptr<AssetDatabase> assetDatabase;

private:
    std::shared_ptr<spdlog::sinks::ringbuffer_sink<std::mutex>> ringBufferLoggerSink;
    std::vector<std::unique_ptr<GameLoop>> gameLoops;
    std::unique_ptr<Physics> physics;

    std::filesystem::path projectPath;
    std::filesystem::path projectAssetPath;
};
