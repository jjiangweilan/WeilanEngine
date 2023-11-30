#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Importers.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Core/Time.hpp"
#include "Event/Event.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline.hpp"
#include "Rendering/Shaders.hpp"
#include <filesystem>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

namespace Engine
{
class WeilanEngine
{
public:
    WeilanEngine() {}
    ~WeilanEngine();

public:
    struct CreateInfo
    {
        std::filesystem::path projectPath;
    };

    void Init(const CreateInfo& createInfo);

    bool BeginFrame();
    void EndFrame();
    Gfx::CommandBuffer& GetActiveCmdBuffer();

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
    class FrameCmdBuffer
    {
    public:
        FrameCmdBuffer(Gfx::GfxDriver& gfxDriver);
        Gfx::CommandBuffer* GetActive()
        {
            return activeCmd;
        }
        void Swap();

    private:
        Gfx::CommandBuffer* activeCmd;
        std::unique_ptr<Gfx::CommandBuffer> cmd0;
        std::unique_ptr<Gfx::CommandBuffer> cmd1;
        std::unique_ptr<Gfx::CommandPool> cmdPool;
    };

    std::unique_ptr<FrameCmdBuffer> frameCmdBuffer;
    std::unique_ptr<RenderPipeline> renderPipeline;
    std::shared_ptr<spdlog::sinks::ringbuffer_sink<std::mutex>> ringBufferLoggerSink;

    std::filesystem::path projectPath;
    std::filesystem::path projectAssetPath;
};
} // namespace Engine
