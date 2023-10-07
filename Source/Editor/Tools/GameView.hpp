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
    GameView();
    ~GameView() override{};

    Camera* gameCamera;
    Camera* editorCamera;

public:
    std::vector<std::string> GetToolMenuItem() override
    {
        return {"View", "Game"};
    }

    void Init();

    bool Tick() override;

    void Render(Gfx::CommandBuffer& cmd, Scene* scene);

private:
    std::unique_ptr<SceneRenderer> renderer;
    std::unique_ptr<Gfx::Image> sceneImage;
    std::unique_ptr<GameObject> editorCameraGO;

    struct
    {
        glm::ivec2 resolution;
    } d; // data

    void CreateRenderData(uint32_t width, uint32_t height);
    void EditTransform(const Camera& camera, glm::mat4& matrix, const glm::vec4& rect);
};
} // namespace Engine::Editor
