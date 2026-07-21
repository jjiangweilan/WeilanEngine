#pragma once
#include "Editor/EditorContext.hpp"
#include "Editor/Gizmos/Gizmo.hpp"
#include "Editor/Gizmos/GizmoManager.hpp"
#include "Editor/SceneEditorTool.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/SceneManager/SceneManager.hpp"
#include "Engine/ThirdParty/imgui/ImGuizmo.h"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <nlohmann/json.hpp>
#include <list>
#include <vector>

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

    void ResetGizmoState();
    bool Tick();

    void Render(Gfx::CommandBuffer& cmd);

    void SetActiveScene(ObjPtr<Scene> scene);
    void SetActiveTool(SceneEditorTool* tool);
    void LoadEditorState(const nlohmann::json& editorState);
    void SaveEditorState(nlohmann::json& editorState) const;

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
    bool gizmoTransformTransactionActive = false;
    std::vector<UUID> gizmoTransformTransactionSelection;
    bool isVisible = false;
    bool pixelZoomEnabled = false;
    bool showGizmos = true;
    bool showTransformGizmos = true;
    bool showSelectionOutline = true;
    bool showHoverHighlightOutline = false;
    std::list<PendingDelete> pendingDeleteSceneImages;
    std::unique_ptr<GameObject> editorCameraGO;
    Gfx::Image* graphOutputImage = nullptr;
    bool useViewCamera = true;
    bool isAltDown = false;
    float editorCameraSpeed = 5.0f;
    Gfx::RenderPass editorFinalColorBlitPass = Gfx::RenderPass(1, 1);
    ObjPtr<Shader> editorFinalColorBlitShader;
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
        glm::vec3 moveVelocity = glm::vec3(0.0f);
        glm::vec2 lookVelocity = glm::vec2(0.0f);
        float yaw = 0.0f;
        float pitch = 0.0f;
        bool anglesInitialized = false;
    } cameraLookAroundContext; // when user press mouse right click, a camera look around context is initialized
                               // it provides some recording while user is moving the camera around

    struct EditorWorldSpaceGrid
    {
        bool show = false;
        // note: line width is hard coded in shader

        // expecting 1x1m, origin in the center of the geometry
        Mesh* plane;
        ObjPtr<Shader> gridShader;
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
    } mouseTrack, middleMouseTrack;

    struct RectSelectContext
    {
        bool isActive = false;
        bool hasDragged = false;
        bool pendingClick = false;
        glm::vec2 startMouse{};
        glm::vec2 currentMouse{};
    } rectSelect;

    std::vector<GameObject*> CollectGameObjectsInRect(Scene& scene, Camera& camera, glm::vec2 uvMin, glm::vec2 uvMax, float imageWidth, float imageHeight);

    int2 sceneImageOrigin{0, 0};

    bool firstFrame = true;
    ObjPtr<Camera> gameCamera = nullptr;
    ObjPtr<Camera> editorCamera = nullptr;
    ObjPtr<Shader> outlineRawColorPassShader;
    ObjPtr<Shader> outlineFullScreenPassShader;
    std::unique_ptr<Gfx::ShaderResource> outlineGPUResource;

    Gfx::ImageIdentifier outlineSrcRT;
    Gfx::RenderPass outlineSrcPass = Gfx::RenderPass::SingleColor();
    Gfx::RenderPass gameImagePass = Gfx::RenderPass::Default(
        "gameImage pass",
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    );

    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE currentGizmoMode = ImGuizmo::LOCAL;
    GizmoHandle gridGizmo;
    SceneEditorTool* activeTool = nullptr;

    void CreateRenderData(uint32_t width, uint32_t height);
    void EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& deltaMatrix, glm::mat4 proj);
    void ChangeGameScreenResolution(glm::ivec2 resolution);
    void FocusOnObject(Camera& camera, GameObject& gameObject);
    Camera* GetCurrentlyActiveCamera();
    bool EditorCameraWalkAround(Camera& editorCamera, float& editorCameraSpeed);
    void RenderObjectToOutlineRT(Gfx::CommandBuffer& cmd, GameObject*& go, int colorType);
    void DrawOutlineAndGizmos(Gfx::CommandBuffer& cmd, Gfx::Image* sceneImage, const Gfx::ImageIdentifier* gameImage, const Gfx::ImageIdentifier* gameDepthImage);
};
} // namespace Editor
