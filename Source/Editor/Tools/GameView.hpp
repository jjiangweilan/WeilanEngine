#pragma once
#include "../Tool.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"

namespace Editor
{

class GameView : public Tool
{
public:
    GameView();
    ~GameView() override{};

public:
    std::vector<std::string> GetToolMenuItem() override
    {
        return {"View", "Game"};
    }

    void Init();

    bool Tick() override;

    void Render(Gfx::CommandBuffer& cmd, Scene* scene);

    Camera* GetEditorCamera() const
    {
        return editorCamera;
    }

private:
    std::unique_ptr<Gfx::Image> sceneImage;
    std::unique_ptr<GameObject> editorCameraGO;
    Gfx::Image* graphOutputImage = nullptr;
    bool useViewCamera = true;

    struct
    {
        glm::ivec2 resolution;
    } d; // data

    Camera* gameCamera = nullptr;
    Camera* editorCamera = nullptr;

    void CreateRenderData(uint32_t width, uint32_t height);
    void EditTransform(Camera& camera, glm::mat4& matrix, const glm::vec4& rect);
};
} // namespace Editor
