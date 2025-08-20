#pragma once
#include "Core/Gizmo.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "Editor/EditorContext.hpp"
#include "Editor/Gizmos/GizmoManager.hpp"
#include "Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Rendering/Shader.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"
#include <list>

namespace Editor
{
class SceneEditor
{
public:
    SceneEditor();
    ~SceneEditor();

public:
    void Init(EditorContext* editorContext);
    void Deinit();

    bool Tick();

    void Render(Gfx::CommandBuffer& cmd);

    void SetActiveScene(ObjPtr<Scene> scene);

    Camera* GetEditorCamera() const { return editorCamera; }

    Gfx::Image* GetSceneImage() { return sceneImage.get(); }
    int2 GetSceneImageOrigin() const { return sceneImageOrigin; }

    EditorContext* editorContext;

private:
    std::unique_ptr<Rendering::RenderPipeline> renderPipeline;
    std::unique_ptr<Gfx::Image> sceneImage;
    GizmoManager* gizmoManager;

    // scene image can't be deleted immediately because it's tracked by the VKDriver for at least two frame (it holds a
    // pointer and doesn't check its validity)
    struct PendingDelete
    {
        std::unique_ptr<Gfx::Image> image;
        int frameCount = 0;
    };
    bool activeViewGizmos = false;
    bool isVisible = false;
    std::list<PendingDelete> pendingDeleteSceneImages;
    std::unique_ptr<GameObject> editorCameraGO;
    Gfx::Image* graphOutputImage = nullptr;
    bool useViewCamera = true;
    bool isAltDown = false;
    float editorCameraSpeed = 5.0f;
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

    struct CameraLookAroundContext
    {
        bool isActive = false;
        float3 startPos = {0, 0, 0};
    } cameraLookAroundContext; // when user press mouse right click, a camera look around context is initialized
                               // it provides some recording while user is moving the camera around

    struct EditorWorldSpaceGrid
    {
        bool show = false;
        // note: line width is hard coded in shader

        // expecting 1x1m, origin in the center of the geometry
        Mesh* plane;
        ObjPtr<Shader2> gridShader;
        glm::vec3 pos; // dynamically centered around editor camera
        const glm::vec3 scale = glm::vec3(
            50, 1, 50
        ); // the plane's mesh center is it's geometry center, so scale 50 to scale the plane by 100
    } editorWorldSpaceGrid = {};

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

    int2 sceneImageOrigin{0, 0};

    bool firstFrame = true;
    ObjPtr<Camera> gameCamera = nullptr;
    ObjPtr<Camera> editorCamera = nullptr;
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
    GizmoHandle gridGizmo;

    void CreateRenderData(uint32_t width, uint32_t height);
    void EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& deltaMatrix, glm::mat4 proj);
    void ChangeGameScreenResolution(glm::ivec2 resolution);
    void FocusOnObject(Camera& camera, GameObject& gameObject);
    Camera* GetCurrentlyActiveCamera();
    void EditorCameraWalkAround(Camera& editorCamera, float& editorCameraSpeed);
};
} // namespace Editor
