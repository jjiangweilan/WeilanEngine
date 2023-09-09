#pragma once
#include "../Tool.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Rendering/SceneRenderer.hpp"

namespace Engine::Editor
{

class GameView : public Tool
{
public:
    GameView(SceneManager& sceneManager);
    ~GameView() override{};

public:
    std::vector<std::string> GetToolMenuItem() override
    {
        return {"View", "Game"};
    }

    bool Tick() override;

    void Render(Gfx::CommandBuffer& cmd) override;

private:
    SceneManager& sceneManager;
    Scene* scene = nullptr;
    std::unique_ptr<SceneRenderer> renderer;
    std::unique_ptr<Gfx::Image> sceneImage;

    void CreateRenderData(uint32_t width, uint32_t height);
    void Render();
};
} // namespace Engine::Editor
