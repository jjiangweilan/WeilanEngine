#pragma once
#include "../Tool.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/SceneRenderer.hpp"

namespace Engine::Editor
{

class EnvironmentBaker : public Tool
{
public:
    EnvironmentBaker();
    ~EnvironmentBaker() override{};

public:
    std::vector<std::string> GetToolMenuItem() override
    {
        return {"Tools", "Scene", "Environment Baker"};
    }

    bool Tick() override;

private:
    std::unique_ptr<Scene> scene;
    std::unique_ptr<SceneRenderer> renderer;
    std::unique_ptr<Gfx::Image> sceneImage;

    void CreateRenderData(uint32_t width, uint32_t height);
};
} // namespace Engine::Editor
