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

    std::unique_ptr<Model2> ImportModel(const char* path)
    {
        return Engine::Importers::GLB(path, sceneRenderer->GetOpaqueShader());
    }

    void Init(const CreateInfo& createInfo);

    void BeginFrame();
    void EndFrame();
    Gfx::CommandBuffer& GetActiveCmdBuffer();

    Asset* LoadAsset(const char* path);

    std::vector<std::function<void(SDL_Event& event)>> eventCallback;
    std::unique_ptr<Event> event;
    std::unique_ptr<Gfx::GfxDriver> gfxDriver;
    std::unique_ptr<AssetDatabase> assetDatabase;
    std::unique_ptr<SceneRenderer> sceneRenderer;

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
};
} // namespace Engine
