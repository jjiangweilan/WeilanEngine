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
    GameSceneCamera(glm::vec3 initialPos, glm::quat initialRotation);
    void Activate(bool gameCamPos);
    void Deactivate();

    void Tick(glm::vec2 mouseInSceneViewUV);
    bool IsActive() { return isActive; }
    glm::vec3 GetCameraPos() { return camera->GetGameObject()->GetTransform()->GetPosition(); }
    glm::quat GetCameraRotation() { return camera->GetGameObject()->GetTransform()->GetRotationQuat(); }

private:
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

    glm::vec3 lastActivePos;
    glm::quat lastActiveRotation;
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
    void RenderSceneGUI(RefPtr<CommandBuffer> cmdBuf);
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
