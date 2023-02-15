#pragma once

#include "../EditorContext.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Editor/Window/EditorWindow.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/Image.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
class GameSceneHandle
{
public:
    virtual void DrawHandle(RefPtr<CommandBuffer> cmdBuf) = 0;
    virtual void Interact(RefPtr<GameObject> go, glm::vec2 mouseInSceneViewUVSpace) = 0;
    virtual std::string GetNameID() = 0;
};

class GameSceneCamera
{
public:
    GameSceneCamera(glm::vec3 initialPos);
    void Activate(bool gameCamPos);
    void Deactivate();

    void Tick(glm::vec2 mouseInSceneViewUV);
    bool IsActive() { return isActive; }
    glm::vec3 GetCameraPos() { return camera->GetGameObject()->GetTransform()->GetPosition(); }
    void GetCameraRotation(float& theta, float& phi)
    {
        theta = this->theta;
        phi = this->phi;
    }

    void SetCameraRotation(float theta, float phi)
    {
        this->theta = theta;
        this->phi = phi;

        UpdateRotation();
    }

private:
    void UpdateRotation()
    {
        auto transform = camera->GetGameObject()->GetTransform();

        phi = glm::clamp(phi, 0.01f, glm::pi<float>() - 0.01f);

        float x = glm::sin(phi) * glm::cos(theta);
        float y = glm::cos(phi);
        float z = glm::sin(phi) * glm::sin(theta);

        auto rot = glm::quatLookAtRH(glm::vec3(x, y, z), {0., 1., 0.});
        transform->SetRotation(rot);
    }

    UniPtr<GameObject> gameObject;
    RefPtr<Camera> camera;
    RefPtr<Camera> oriCam;
    bool isActive = false;

    enum class OperationType
    {
        None,
        Pan,
        Move,
        LookAround
    } operationType;

    float phi = 0;
    float theta = 0;
    glm::vec3 lastActivePos;
    glm::ivec2 initialMousePos;
    ImVec2 imguiInitialMousePos;
};

class GameSceneWindow : public EditorWindow
{
public:
    GameSceneWindow(RefPtr<EditorContext> editorContext);
    // image: the image to display at this window
    void Tick() override;
    void OnDestroy() override;
    const char* GetWindowName() override { return "Game Scene"; }
    Gfx::Image* GetGameSceneImageTarget() { return gameSceneImage.Get(); };
    ImGuiWindowFlags GetWindowFlags() override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar; }

private:
    RefPtr<EditorContext> editorContext;

    GameSceneCamera gameSceneCam;

    /* Rendering */
    RefPtr<Gfx::Image> sceneColor;
    RefPtr<Gfx::Image> sceneDepth;
    RefPtr<Shader> blendBackShader;

    UniPtr<Gfx::Image> gameSceneImage;

    UniPtr<Gfx::Image> newSceneColor;
    UniPtr<Gfx::RenderPass> handlePass;
    UniPtr<Gfx::RenderPass> outlineFullScreenPass;
    UniPtr<Gfx::RenderPass> blendBackPass;
    UniPtr<Gfx::ShaderResource> blendBackResource;
    UniPtr<GameSceneHandle> activeHandle;

    UniPtr<Gfx::Image> editorOverlayColor;
    UniPtr<Gfx::Image> editorOverlayDepth;
    UniPtr<Gfx::Image> outlineOffscreenColor;
    UniPtr<Gfx::Image> outlineOffscreenDepth;

    UniPtr<Gfx::RenderPass> outlinePass;
    UniPtr<Gfx::ShaderResource> outlineResource;

    RefPtr<Shader> outlineRawColor, outlineFullScreen;

    void UpdateRenderingResources(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth);
};
} // namespace Engine::Editor
