#pragma once
#include "Core/Gizmo.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Rendering/Shader.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"
#include <list>

namespace Editor
{
class GameView
{
public:
    GameView();
    ~GameView();

public:
    void Init();
    void Deinit();

    bool Tick();

    void Render(
        Gfx::CommandBuffer& cmd,
        const Gfx::RG::ImageIdentifier* gameImage,
        const Gfx::RG::ImageIdentifier* gameDepthImage
    );

    Gfx::Image* GetGameScreenImage() { return sceneImage.get(); }

private:
    std::unique_ptr<Gfx::Image> sceneImage;

    // scene image can't be deleted immediately because it's tracked by the VKDriver for at least two frame (it holds a
    // pointer and doesn't check its validity)
    struct PendingDelete
    {
        std::unique_ptr<Gfx::Image> image;
        int frameCount = 0;
    };
    std::list<PendingDelete> pendingDeleteSceneImages;
    Gfx::Image* graphOutputImage = nullptr;
    bool isAltDown = false;
    Gfx::RG::RenderPass editorFinalColorBlitPass = Gfx::RG::RenderPass(1, 1);
    ObjPtr<Shader2> editorFinalColorBlitShader;
    std::unique_ptr<Material> editorFinalColorBlitMaterial;

    struct
    {
        glm::ivec2 resolution;
    } d; // data

    struct GameObjectConfigs
    {
        bool useSnap = false;
        glm::vec3 snap = glm::vec3(0.25f);
    } gameObjectConfigs = {};

    struct MouseDelta
    {
        bool isTracking = false;
        glm::float2 lastMousePos;

        glm::float2 GetMouseDelta(ImGuiMouseButton mouseButton)
        {
            if (ImGui::IsMouseDown(mouseButton))
            {
                if (isTracking == false)
                {
                    isTracking = true;
                    lastMousePos = ImGui::GetMousePos();
                    return glm::float2(0, 0);
                }
                else
                {
                    auto currentPos = ImGui::GetMousePos();
                    auto delta = currentPos - lastMousePos;
                    lastMousePos = currentPos;
                    return {delta.x, -delta.y};
                }
            }
            if (ImGui::IsMouseReleased(mouseButton))
            {
                isTracking = false;
            }

            return {0, 0};
        }
    } mouseTrack;

    struct PlayTheGame;
    std::unique_ptr<PlayTheGame> playTheGame;

    bool firstFrame = true;
    ObjPtr<Camera> gameCamera = nullptr;
    ObjPtr<Shader2> outlineRawColorPassShader;
    ObjPtr<Shader2> outlineFullScreenPassShader;
    std::unique_ptr<Gfx::ShaderResource> outlineGPUResource;

    Gfx::RG::ImageIdentifier outlineSrcRT;
    Gfx::RG::RenderPass outlineSrcPass = Gfx::RG::RenderPass::SingleColor();
    Gfx::RG::RenderPass gameImagePass = Gfx::RG::RenderPass::Default(
        "gameImage pass",
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    );

    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE currentGizmoMode = ImGuizmo::LOCAL;

    void CreateRenderData(uint32_t width, uint32_t height);
    void ChangeGameScreenResolution(glm::ivec2 resolution);
};
} // namespace Editor
