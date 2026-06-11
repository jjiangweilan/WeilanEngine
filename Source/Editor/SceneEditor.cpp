#include "SceneEditor.hpp"

#include "Editor/EditorCameraControllerMath.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/Gizmos/Gizmo.hpp"
#include "Editor/HudDebug.hpp"
#include "Editor/PickObjectFromGameView.hpp"
#include "Editor/SceneEditorTool.hpp"
#include "Engine/Core/EngineState.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/Physics/JoltDebugRenderer.hpp"
#include "Engine/Game/Input.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/MiddleLayer/DebugOptions.hpp"
#include "Engine/MiddleLayer/SystemInfo.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <limits>

namespace Editor
{

namespace
{
struct SelectionRectNDC
{
    float minX;
    float maxX;
    float minY;
    float maxY;
};

bool VertexInsideSelectionRect(const glm::vec4& clip, const SelectionRectNDC& rect)
{
    constexpr float epsilon = 1e-5f;
    if (clip.w <= epsilon)
        return false;
    return clip.x >= rect.minX * clip.w && clip.x <= rect.maxX * clip.w &&
           clip.y >= rect.minY * clip.w && clip.y <= rect.maxY * clip.w;
}

bool TriangleFullyInsideSelectionRect(
    const glm::vec3& p0,
    const glm::vec3& p1,
    const glm::vec3& p2,
    const glm::mat4& mvp,
    const SelectionRectNDC& rect
)
{
    glm::vec4 c0 = mvp * glm::vec4(p0, 1.0f);
    glm::vec4 c1 = mvp * glm::vec4(p1, 1.0f);
    glm::vec4 c2 = mvp * glm::vec4(p2, 1.0f);
    return VertexInsideSelectionRect(c0, rect) &&
           VertexInsideSelectionRect(c1, rect) &&
           VertexInsideSelectionRect(c2, rect);
}

bool ProjectedAABBOverlapsSelectionRect(const AABB& aabb, const glm::mat4& mvp, const SelectionRectNDC& rect)
{
    glm::vec3 corners[] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.max.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.max.z},
    };

    glm::vec2 screenMin(std::numeric_limits<float>::max());
    glm::vec2 screenMax(std::numeric_limits<float>::lowest());
    bool anyInFront = false;

    for (int i = 0; i < 8; ++i)
    {
        glm::vec4 clip = mvp * glm::vec4(corners[i], 1.0f);
        if (clip.w <= 0.0f)
            continue;
        anyInFront = true;

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        screenMin = glm::min(screenMin, glm::vec2(ndc.x, ndc.y));
        screenMax = glm::max(screenMax, glm::vec2(ndc.x, ndc.y));
    }

    if (!anyInFront)
        return false;

    return screenMin.x <= rect.maxX && screenMax.x >= rect.minX && screenMin.y <= rect.maxY && screenMax.y >= rect.minY;
}

bool ProjectedAABBInsideSelectionRect(const AABB& aabb, const glm::mat4& mvp, const SelectionRectNDC& rect)
{
    for (int i = 0; i < 8; ++i)
    {
        glm::vec3 corner = glm::vec3(
            (i & 1) ? aabb.max.x : aabb.min.x,
            (i & 2) ? aabb.max.y : aabb.min.y,
            (i & 4) ? aabb.max.z : aabb.min.z
        );
        if (!VertexInsideSelectionRect(mvp * glm::vec4(corner, 1.0f), rect))
            return false;
    }
    return true;
}
} // namespace

SceneEditor::SceneEditor() {}
SceneEditor::~SceneEditor()
{ }

void SceneEditor::Deinit() {}

void SceneEditor::SetActiveTool(SceneEditorTool* tool)
{
    if (activeTool)
        activeTool->OnDeactivate();
    activeTool = tool;
    if (activeTool)
        activeTool->OnActivate();
}

void SceneEditor::SetActiveScene(ObjPtr<Scene> scene)
{
    if (scene)
    {
        SceneManager::SetActiveScene(scene.Get());
        editorCamera->GetGameObject()->SetScene(SceneManager::GetActiveScene());
    }
}

void SceneEditor::Init(EditorContext* editorContext)
{
    this->editorContext = editorContext;
    this->gizmoManager = editorContext->GetGizmoManager();

    renderPipeline = std::make_unique<Rendering::RenderPipeline>();
    editorCameraGO = std::make_unique<GameObject>();

    editorCameraGO->SetName("editor camera");
    editorCamera = editorCameraGO->AddComponent<Camera>();

    editorContext->SetEditorCamera(editorCamera.Get());

    outlineGPUResource = GetGfxDriver()->CreateShaderResource();
    const char* editorFinalColorBlitShaderKeyword[] = {"_ResetAlpha"};
    editorFinalColorBlitShader = ShaderLibrary::GetShader(
        "Blit",
        ShaderLibrary::QueryShaderFeatures("Blit").GetPermutation(editorFinalColorBlitShaderKeyword)
    );
    editorFinalColorBlitMaterial = std::make_unique<Material>();
    editorFinalColorBlitMaterial->SetShader(editorFinalColorBlitShader);

    Gfx::SubpassAttachment color = {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::SubpassAttachment colorVec[] = {color};
    editorFinalColorBlitPass.SetSubpass(0, colorVec);

    // setup camera state
    if (auto scene = SceneManager::GetActiveScene())
    {
        SetActiveScene(scene);
    }

    if (GameEditor::instance->editorState.contains("editorCamera"))
    {
        auto& camJson = GameEditor::instance->editorState["editorCamera"];
        std::array<float, 3> pos{0, 0, 0};
        std::array<float, 4> rot{1, 0, 0, 0};
        std::array<float, 3> scale{1, 1, 1};
        try
        {
            pos = camJson.value("position", pos);
            rot = camJson.value("rotation", rot);
            scale = camJson.value("scale", scale);
        }
        catch (...)
        {
            pos = {0, 0, 0};
            rot = {1, 0, 0, 0};
            scale = {1, 1, 1};
        }

        editorCamera->GetGameObject()->SetPosition({pos[0], pos[1], pos[2]});
        editorCamera->GetGameObject()->SetRotation(glm::quat{rot[0], rot[1], rot[2], rot[3]});
        editorCamera->GetGameObject()->SetScale({scale[0], scale[1], scale[2]});
    }

    {
        auto angles = ComputeEditorCameraAnglesFromForward(editorCamera->GetForward(), cameraLookAroundContext.yaw);
        cameraLookAroundContext.yaw = angles.yaw;
        cameraLookAroundContext.pitch = angles.pitch;
        cameraLookAroundContext.anglesInitialized = true;
    }

    outlineRawColorPassShader = ShaderLibrary::GetShader(Shaders::PostProcess_OutlineRawColorPass);
    outlineFullScreenPassShader = ShaderLibrary::GetShader(Shaders::PostProcess_OutlineFullScreenPass);

    editorWorldSpaceGrid.plane =
        static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.fbx"))
            ->GetMeshes()[0]
            .get();
    editorWorldSpaceGrid.gridShader = ShaderLibrary::GetShader(Shaders::PlaneGrid);

    ChangeGameScreenResolution({256, 256});
}

bool SceneEditor::EditorCameraWalkAround(Camera& editorCamera, float& editorCameraSpeed)
{
    const bool isWindowHovered = ImGui::IsWindowHovered();
    const bool isMouseRightButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool isMiddleButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool allowLookControl = isWindowHovered || cameraLookAroundContext.isActive;
    const bool allowPanControl = isWindowHovered || middleMouseTrack.isTracking;

    if (!allowLookControl && !allowPanControl)
        return false;

    auto mouseDelta = mouseTrack.GetMouseDelta(ImGuiMouseButton_Right);
    auto middleMouseDelta = middleMouseTrack.GetMouseDelta(ImGuiMouseButton_Middle);

    bool moved = false;
    const float deltaTime = glm::min(Time::DeltaTime(), 0.05f);

    auto go = editorCamera.GetGameObject();
    auto pos = go->GetPosition();

    if (!cameraLookAroundContext.anglesInitialized)
    {
        auto angles = ComputeEditorCameraAnglesFromForward(editorCamera.GetForward(), cameraLookAroundContext.yaw);
        cameraLookAroundContext.yaw = angles.yaw;
        cameraLookAroundContext.pitch = angles.pitch;
        cameraLookAroundContext.anglesInitialized = true;
    }

    glm::vec3 forward = editorCamera.GetForward();

    float mouseWheel = ImGui::GetIO().MouseWheel;
    if (isWindowHovered && isAltDown)
    {
        if (mouseWheel != 0.0f)
        {
            if (editorCameraSpeed <= 1)
            {
                if (editorCameraSpeed < 0.1)
                {
                    editorCameraSpeed += mouseWheel * 0.01f;
                }
                else
                    editorCameraSpeed += mouseWheel * 0.1f;
            }
            else
            {
                editorCameraSpeed += mouseWheel;
            }
            editorCameraSpeed = glm::max(editorCameraSpeed, 0.001f);
            spdlog::info("change editor camera speed to {}", editorCameraSpeed);
        }
    }
    else if (isWindowHovered && mouseWheel != 0.0f)
    {
        float zoomSpeed = editorCameraSpeed * 0.5f;
        pos += forward * zoomSpeed * mouseWheel;
        go->SetPosition(pos);
        moved = true;
    }

    if (isMouseRightButtonDown)
    {
        if (!cameraLookAroundContext.isActive)
        {
            cameraLookAroundContext.isActive = true;
            cameraLookAroundContext.startPos = pos;
            cameraLookAroundContext.lookVelocity = glm::vec2(0.0f);

            auto angles = ComputeEditorCameraAnglesFromForward(editorCamera.GetForward(), cameraLookAroundContext.yaw);
            cameraLookAroundContext.yaw = angles.yaw;
            cameraLookAroundContext.pitch = angles.pitch;
        }

        if (glm::length2(mouseDelta) > 0.0f)
        {
            constexpr float mouseSensitivity = 0.0035f;
            constexpr float lookSharpness = 24.0f;
            constexpr float pitchLimit = glm::radians(89.0f);
            glm::vec2 lookInput(-mouseDelta.x, mouseDelta.y);
            lookInput *= mouseSensitivity;

            float lookBlend = 1.0f - glm::exp(-lookSharpness * deltaTime);
            cameraLookAroundContext.lookVelocity = glm::mix(cameraLookAroundContext.lookVelocity, lookInput, lookBlend);
            cameraLookAroundContext.yaw += cameraLookAroundContext.lookVelocity.x;
            cameraLookAroundContext.pitch = glm::clamp(
                cameraLookAroundContext.pitch + cameraLookAroundContext.lookVelocity.y,
                -pitchLimit,
                pitchLimit
            );

            go->SetRotation(BuildEditorCameraRotation(cameraLookAroundContext.yaw, cameraLookAroundContext.pitch));
            moved = true;
        }
        else
        {
            cameraLookAroundContext.lookVelocity = glm::vec2(0.0f);
        }

        glm::vec3 right = go->GetRight();
        glm::vec3 up = go->GetUp();
        forward = editorCamera.GetForward();

        glm::vec3 moveInput(0.0f);
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            moveInput += right;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            moveInput -= right;
        }
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            moveInput += forward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            moveInput -= forward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            moveInput += up;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            moveInput -= up;
        }

        if (glm::length2(moveInput) > 0.0f)
        {
            moveInput = glm::normalize(moveInput);
        }

        const float blendSpeed = glm::length2(moveInput) > 0.0f ? 16.0f : 22.0f;
        const float moveBlend = 1.0f - glm::exp(-blendSpeed * deltaTime);
        glm::vec3 targetVelocity = moveInput * editorCameraSpeed;
        cameraLookAroundContext.moveVelocity = glm::mix(cameraLookAroundContext.moveVelocity, targetVelocity, moveBlend);

        if (glm::length2(cameraLookAroundContext.moveVelocity) < 1e-4f)
        {
            cameraLookAroundContext.moveVelocity = glm::vec3(0.0f);
        }

        if (glm::length2(cameraLookAroundContext.moveVelocity) > 0.0f)
        {
            pos += cameraLookAroundContext.moveVelocity * deltaTime;
            go->SetPosition(pos);
            moved = true;
        }
    }
    else if (isMiddleButtonDown)
    {
        if (glm::length2(middleMouseDelta) > 0.0f)
        {
            float panSpeed = editorCameraSpeed * 0.01f;

            pos = go->GetPosition();

            pos -= (go->GetRight() * (middleMouseDelta.x * panSpeed) + go->GetUp() * (middleMouseDelta.y * panSpeed));

            go->SetPosition(pos);
            moved = true;
        }
    }

    if (!isMouseRightButtonDown)
    {
        cameraLookAroundContext.isActive = false;
        cameraLookAroundContext.moveVelocity = glm::vec3(0.0f);
        cameraLookAroundContext.lookVelocity = glm::vec2(0.0f);
    }

    // Print the current position of the editor camera to the HUD
    pos = editorCamera.GetGameObject()->GetPosition();
    HudDebug::Print(fmt::format("{}, {}, {}", pos.x, pos.y, pos.z));

    return moved;
}

void SceneEditor::CreateRenderData(uint32_t width, uint32_t height)
{
    pendingDeleteSceneImages.push_back({std::move(sceneImage), 0});

    sceneImage = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R8G8B8A8_SRGB),
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );

    d.resolution = {width, height};
    editorCamera->SetFoV(glm::radians(60.0f));
    editorCamera->SetNear(0.01f);
    editorCamera->SetFar(25000.f);
}

void SceneEditor::Render(Gfx::CommandBuffer& cmd)
{
    auto scene = SceneManager::GetActiveScene();
    if (scene == nullptr || !isVisible)
        return;

    glm::float4 renderPassLabelColor{0.4, 0.5, 0.13, 1.0};

    cmd.BeginLabel("Scene Editor View", &renderPassLabelColor[0]);
    ImVec2 mousePos = ImGui::GetMousePos();
    glm::vec2 relMousePos = {mousePos.x - sceneImageOrigin.x, mousePos.y - sceneImageOrigin.y};
    Rendering::RenderConfig renderConfig = {
        .drawGraphics = true,
        .cmdOverride = &cmd,
        .enablePixelZoom = pixelZoomEnabled,
        .pixelZoomMousePos = relMousePos
    };
    renderPipeline->SetConfig(renderConfig);
    renderPipeline->Render(*scene, *editorCamera, d.resolution);
    auto gameImage = &renderPipeline->GetOutputColor();
    auto gameDepthImage = &renderPipeline->GetOutputDepth();

    DrawOutlineAndGizmos(cmd, sceneImage.get(), gameImage, gameDepthImage);

    auto outputImage = GetGfxDriver()->GetImageFromRenderGraph(*gameImage);
    if (outputImage)
    {
        cmd.BeginLabel("Editor FinalColorBlit", &renderPassLabelColor[0]);
        {
            editorFinalColorBlitMaterial->SetTexture("input", outputImage);
            Gfx::ClearValue clear[] = {{0, 0, 0, 0}};
            editorFinalColorBlitPass.SetAttachment(0, *sceneImage);
            cmd.BeginRenderPass(editorFinalColorBlitPass, clear);
            cmd.BindResource(0, editorFinalColorBlitMaterial->GetShaderResource());
            cmd.BindShaderProgram(
                editorFinalColorBlitShader->GetShaderProgram(),
                editorFinalColorBlitShader->GetShaderProgram()->GetDefaultShaderConfig()
            );
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }
        cmd.EndLabel();
    }
    cmd.EndLabel();
}

bool SceneEditor::Tick()
{
    ENGINE_BEGIN_PROFILE("SceneEditor Tick");

    bool cameraDirty = false;
    auto scene = SceneManager::GetActiveScene();
    if (scene == nullptr)
        return false;

    gameCamera = scene->GetMainCamera();

    for (auto& p : pendingDeleteSceneImages)
    {
        p.frameCount += 1;
    }
    pendingDeleteSceneImages.remove_if([](PendingDelete& p)
                                       { return p.frameCount > 5; });

    bool open = true;
    isVisible = ImGui::Begin("Scene", &open, ImGuiWindowFlags_MenuBar);

    if (ImGui::IsWindowHovered())
    {
        // Focus on this window we player want to move the camera, which is triggered by right mouse button
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::FocusWindow(ImGui::FindWindowByName("Scene"));
        }
    }

    // is able gameplay input if user is working on the scene editor
    bool isWindowFocused = ImGui::IsWindowFocused();
    Input::SetGameplayInput(!isWindowFocused);

    // seems like ImGui::IsKeyPressed(ImGuiKey_XXXAlt/XXXShift) most of time can't be registered at the same with with
    // MouseWheel so I track the down and release event individually
    if (ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        isAltDown = true;
    if (ImGui::IsKeyReleased(ImGuiKey_LeftAlt))
        isAltDown = false;

    // sync camera settigns
    if (gameCamera && editorCamera)
    {
        editorCamera->SetDiffuseEnv(gameCamera->GetDiffuseEnv().Get());
        editorCamera->SetSpecularEnv(gameCamera->GetSpecularEnv().Get());
    }

    const char* menuAction = "";
    ImVec2 fovPopupPos;
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Debug Draw"))
        {
            ImGui::Checkbox("Physics", &GetDebugOptions().drawPhysicsColliders);
            ImGui::Checkbox("Physics Queries", &GetDebugOptions().drawPhysicsQueries);
            ImGui::Checkbox("Game Object", &GetDebugOptions().drawGameObjectDebugDraw);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Gizmos", nullptr, &showGizmos);
            ImGui::MenuItem("Toggle Grid", nullptr, &editorWorldSpaceGrid.show);
            ImGui::MenuItem("Selection Outline", nullptr, &showSelectionOutline);
            ImGui::MenuItem("Hover Highlight Outline", nullptr, &showHoverHighlightOutline);
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Pixel Zoom", nullptr, &pixelZoomEnabled);
        fovPopupPos = ImGui::GetCursorScreenPos();
        fovPopupPos.y += ImGui::GetFrameHeight();
        if (ImGui::MenuItem("Camera FoV"))
        {
            menuAction = "Camera FoV";
        }
        ImGui::EndMenuBar();
    }

    if (strcmp(menuAction, "Camera FoV") == 0)
    {
        ImGui::OpenPopup("Camera FoV");
    }

    if (ImGui::BeginPopup("Camera FoV"))
    {
        ImGui::SetWindowPos(fovPopupPos, ImGuiCond_Always);
        float fovDegrees = glm::degrees(editorCamera->GetFoV());
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("##FoVSlider", &fovDegrees, 1.0f, 179.0f, "%.1f"))
        {
            editorCamera->SetFoV(glm::radians(fovDegrees));
        }
        ImGui::EndPopup();
    }

    // alway match window size
    int width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    int height = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    if (sceneImage)
    {
        if (firstFrame || width != sceneImage->GetDescription().width || height != sceneImage->GetDescription().height)
        {
            firstFrame = false;
            ChangeGameScreenResolution({width, height});
        }
    }

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C) && ImGui::IsWindowFocused())
    {
        if (Scene* scene = SceneManager::GetActiveScene())
        {
            if (GameObject* selected = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject()))
            {
                EditorState::SelectObject(scene->CopyGameObject(*selected));
            }
        }
    }

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_F, 0))
    {
        if (GameObject* go = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject()))
        {
            if (auto mainCam = editorCamera)
            {
                auto m = mainCam->GetGameObject()->GetWorldMatrix();
                go->SetWorldMatrix(m);
            }
        }
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F))
    {
        if (GameObject* go = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject()))
        {
            if (auto mainCam = editorCamera)
            {
                FocusOnObject(*mainCam, *go);
                auto angles = ComputeEditorCameraAnglesFromForward(mainCam->GetForward(), cameraLookAroundContext.yaw);
                cameraLookAroundContext.yaw = angles.yaw;
                cameraLookAroundContext.pitch = angles.pitch;
                cameraLookAroundContext.anglesInitialized = true;
                cameraDirty = true;
            }
        }
    }

    cameraDirty |= EditorCameraWalkAround(*editorCamera, editorCameraSpeed);

    // create scene color if it's null or if the window size is changed
    const auto contentMax = ImGui::GetWindowContentRegionMax();
    const auto contentMin = ImGui::GetWindowContentRegionMin();
    const float contentWidth = contentMax.x - contentMin.x;
    const float contentHeight = contentMax.y - contentMin.y;

    if (sceneImage && sceneImage->GetDescription().width > 0 && sceneImage->GetDescription().height > 0)
    {
        float imageWidth = sceneImage->GetDescription().width;
        float imageHeight = sceneImage->GetDescription().height;

        // shrink width
        if (imageWidth > contentWidth)
        {
            float ratio = contentWidth / (float)imageWidth;
            imageWidth = contentWidth;
            imageHeight *= ratio;
        }

        if (imageHeight > contentHeight)
        {
            float ratio = contentHeight / (float)imageHeight;
            imageHeight = contentHeight;
            imageWidth *= ratio;
        }

        auto contentRegionWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        auto cursorPos = ImGui::GetCursorPos();
        cursorPos.x = cursorPos.x + contentRegionWidth / 2.0f - imageWidth / 2.0f;
        ImGui::SetCursorPos(cursorPos);

        auto windowPos = ImGui::GetWindowPos();
        auto imagePos = ImGui::GetCursorPos();

        sceneImageOrigin = int2(windowPos.x + imagePos.x, windowPos.y + imagePos.y);
        editorContext->SetSceneViewRect(glm::int4(sceneImageOrigin.x, sceneImageOrigin.y, imageWidth, imageHeight));

        ImGui::Image(&sceneImage->GetDefaultImageView(), {imageWidth, imageHeight});
        bool isGameViewHovered = ImGui::IsItemHovered();
        bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool isMouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        // Calcualte view gizmo related information
        bool hoveringViewGizmo = false;
        auto cursorX = imageWidth - 105;
        auto cursorY = 5;
        glm::vec4 viewGizmoRect = {imagePos.x + windowPos.x, imagePos.y + windowPos.y, imageWidth, imageHeight};
        auto viewManipulateRectMin = ImVec2(viewGizmoRect.x + cursorX, viewGizmoRect.y + cursorY);
        hoveringViewGizmo = ImGui::IsMouseHoveringRect(viewManipulateRectMin, viewManipulateRectMin + ImVec2(100, 100));

        if (isMouseClicked && hoveringViewGizmo)
        {
            activeViewGizmos = true;
        }

        if (isMouseReleased)
        {
            activeViewGizmos = false;
        }

        auto mousePos = ImGui::GetMousePos();
        glm::vec2 mouseContentPos{mousePos.x - windowPos.x - imagePos.x, mousePos.y - windowPos.y - imagePos.y};
        glm::vec2 screenUV = mouseContentPos / glm::vec2{imageWidth, imageHeight};

        if (isGameViewHovered)
        {
            HudDebug::Print(fmt::format("Mouse Pixel Location: {:.0f}, {:.0f}", mouseContentPos.x, mouseContentPos.y));
        }

        // Compute world ray for tool and picking
        auto mainCam = GetCurrentlyActiveCamera();
        Ray worldRay;
        bool hasWorldRay = false;
        if (mainCam != nullptr)
        {
            worldRay = mainCam->ScreenUVToWorldSpaceRay(screenUV);
            hasWorldRay = true;
        }

        // Active tool dispatch
        bool toolConsumedInput = false;
        if (activeTool && hasWorldRay && isGameViewHovered)
        {
            SceneEditorToolContext ctx{this, screenUV, worldRay, isGameViewHovered, gizmoManager, editorContext};
            toolConsumedInput = activeTool->Tick(ctx);
        }

        if (scene && showGizmos)
        {
            // Gizmo
            scene->ForEachGameObject([this](GameObject* g)
                                     {
                GizmoBase::SetActiveCarrier(g);
                for (auto& c : g->GetComponents())
                {
                    if (c)
                    {
                        c->OnDrawGizmos(); // TODO: remove this
                        c->OnDrawGizmos(*gizmoManager);
                    }
                }
                GizmoBase::ClearActiveCarrier(); });
        }

        bool anyItemHovered = ImGui::IsAnyItemHovered();
        auto selected = EditorState::GetMainSelectedObject();

        // --- Rect selection: track drag start ---
        if (isMouseClicked && isGameViewHovered && ImGui::IsWindowFocused() && !toolConsumedInput &&
            !anyItemHovered && (selected == nullptr || !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) &&
            !hoveringViewGizmo && !gizmoManager->AnyGizmoActive())
        {
            rectSelect.isActive = true;
            rectSelect.hasDragged = false;
            rectSelect.pendingClick = true;
            rectSelect.startMouse = mouseContentPos;
            rectSelect.currentMouse = mouseContentPos;
        }

        if (rectSelect.isActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            rectSelect.currentMouse = mouseContentPos;
            float dragDist = glm::length(rectSelect.currentMouse - rectSelect.startMouse);
            if (dragDist > 4.0f)
            {
                rectSelect.hasDragged = true;
                rectSelect.pendingClick = false;
            }
        }

        if (rectSelect.hasDragged)
        {
                glm::vec2 rectScreenMin = glm::min(rectSelect.startMouse, rectSelect.currentMouse);
                glm::vec2 rectScreenMax = glm::max(rectSelect.startMouse, rectSelect.currentMouse);

            ImVec2 drawMin = ImVec2(sceneImageOrigin.x + rectScreenMin.x, sceneImageOrigin.y + rectScreenMin.y);
            ImVec2 drawMax = ImVec2(sceneImageOrigin.x + rectScreenMax.x, sceneImageOrigin.y + rectScreenMax.y);

            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            drawList->AddRectFilled(drawMin, drawMax, IM_COL32(60, 130, 240, 40));
            drawList->AddRect(drawMin, drawMax, IM_COL32(60, 130, 240, 200), 0.0f, 0, 1.5f);
        }

        if (isMouseReleased && rectSelect.isActive)
        {
            if (rectSelect.hasDragged && scene && mainCam && imageWidth > 0 && imageHeight > 0)
            {
                glm::vec2 imageSize{imageWidth, imageHeight};
                glm::vec2 startUV = glm::clamp(rectSelect.startMouse / imageSize, glm::vec2(0.0f), glm::vec2(1.0f));
                glm::vec2 endUV = glm::clamp(rectSelect.currentMouse / imageSize, glm::vec2(0.0f), glm::vec2(1.0f));

                auto objectsInRect = CollectGameObjectsInRect(*scene, *mainCam, startUV, endUV, imageWidth, imageHeight);

                bool shiftHeld = ImGui::IsKeyDown(ImGuiKey_LeftShift);
                bool altHeld = isAltDown;

                if (altHeld)
                {
                    for (auto* go : objectsInRect)
                        EditorState::DeselectObject(go);
                }
                else if (shiftHeld)
                {
                    for (auto* go : objectsInRect)
                        EditorState::SelectObject(go, true);
                }
                else
                {
                    EditorState::SelectObject(nullptr);
                    for (auto* go : objectsInRect)
                        EditorState::SelectObject(go, !objectsInRect.empty());
                }
            }
            else if (rectSelect.pendingClick && !toolConsumedInput && !anyItemHovered &&
                     (selected == nullptr || !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) &&
                     !hoveringViewGizmo && !gizmoManager->AnyGizmoActive() &&
                     mainCam && scene)
            {
                using Intersected = PickGameObjectFromScene::Intersected;
                Ray ray = worldRay;

                std::vector<Intersected> intersected;
                intersected.reserve(32);
                std::vector<GameObject*> results;
                Gizmos::PickGizmos(ray, results);
                if (results.empty())
                {
                    auto sceneIntersected = PickGameObjectFromScene()(*scene, ray, screenUV);
                    intersected.insert(intersected.end(), sceneIntersected.begin(), sceneIntersected.end());
                }
                else
                {
                    for (auto g : results)
                    {
                        intersected.push_back(
                            {g, glm::length(g->GetPosition() - mainCam->GetGameObject()->GetPosition())}
                        );
                    }
                }

                auto iter = std::min_element(
                    intersected.begin(),
                    intersected.end(),
                    [](const Intersected& l, const Intersected& r)
                    { return l.distance > 0 && l.distance < r.distance; }
                );

                GameObject* picked = nullptr;
                if (iter != intersected.end())
                {
                    picked = iter->go;
                }

                if (picked)
                {
                    auto selectedObjects = EditorState::GetSelectedObjects();
                    auto findIter = std::find_if(
                        selectedObjects.begin(),
                        selectedObjects.end(),
                        [picked](ObjPtr<Object> o)
                        { return o.Get() == picked; }
                    );
                    if (findIter == selectedObjects.end())
                    {
                        bool multiSelect = ImGui::IsKeyDown(ImGuiKey_LeftShift);
                        EditorState::SelectObject(picked, multiSelect);
                    }
                    else
                    {
                        if (isAltDown)
                        {
                            EditorState::DeselectObject(picked);
                        }
                    }
                }
                else
                {
                    EditorState::SelectObject(nullptr);
                }
            }
            rectSelect.isActive = false;
            rectSelect.hasDragged = false;
            rectSelect.pendingClick = false;
        }

        // Draw View Gizmos
        ImGui::SetCursorPos(imagePos);
        if (scene != nullptr && imageWidth > 0 && imageHeight > 0)
        {
            auto mainCam = editorCamera;
            if (mainCam)
            {
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetGizmoSizeClipSpace(0.2f);
                ImGuizmo::SetRect(viewGizmoRect.x, viewGizmoRect.y, viewGizmoRect.z, viewGizmoRect.w);

                glm::mat4 proj = mainCam->GetAndUpdateProjectionMatrix(imageWidth / imageHeight);
                proj[1] *= -1;

                GameObject* go = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject());
                if (go)
                {
                    auto selectedObjects = EditorState::GetSelectedObjects();
                    std::vector<GameObject*> selectedGameObjects;
                    std::vector<UUID> selectedGameObjectUUIDs;
                    selectedGameObjects.reserve(selectedObjects.size());
                    selectedGameObjectUUIDs.reserve(selectedObjects.size());
                    for (auto& selected : selectedObjects)
                    {
                        if (GameObject* selectedGameObject = dynamic_cast<GameObject*>(selected.Get()))
                        {
                            selectedGameObjects.push_back(selectedGameObject);
                            selectedGameObjectUUIDs.push_back(selectedGameObject->GetUUID());
                        }
                    }

                    if (selectedGameObjects.empty())
                    {
                        selectedGameObjects.push_back(go);
                        selectedGameObjectUUIDs.push_back(go->GetUUID());
                    }

                    auto baseModel = go->GetWorldMatrix();

                    glm::vec3 avgPos = glm::vec3(0);

                    for (GameObject* selectedGameObject : selectedGameObjects)
                    {
                        avgPos += selectedGameObject->GetPosition();
                    }
                    avgPos /= selectedGameObjects.size();
                    baseModel[3] = glm::vec4(avgPos, 1.0f);

                    glm::mat4 deltaMatrix;
                    EditTransform(*mainCam, baseModel, deltaMatrix, proj);

                    glm::vec3 deltaPosition;
                    glm::vec3 deltaScale;
                    glm::quat deltaRotation;
                    Math::DecomposeMatrix(deltaMatrix, deltaPosition, deltaScale, deltaRotation);

                    // calculate scale factor here
                    baseModel[0] = glm::normalize(baseModel[0]);
                    baseModel[1] = glm::normalize(baseModel[1]);
                    baseModel[2] = glm::normalize(baseModel[2]);
                    auto baseModelInv = glm::inverse(baseModel);

                    auto deltaTR = glm::translate(glm::mat4(1), deltaPosition) * glm::mat4_cast(deltaRotation);
                    auto deltaS = glm::scale(glm::mat4(1), deltaScale);

                    if (ImGuizmo::IsUsing())
                    {
                        auto& undoManager = EditorState::GetUndoManager();
                        if (gizmoTransformTransactionActive && gizmoTransformTransactionSelection != selectedGameObjectUUIDs)
                        {
                            undoManager.EndTransaction();
                            gizmoTransformTransactionActive = false;
                            gizmoTransformTransactionSelection.clear();
                        }

                        if (!gizmoTransformTransactionActive)
                        {
                            if (!undoManager.HasActiveTransaction())
                            {
                                undoManager.BeginTransaction("Transform GameObject");
                                for (GameObject* selectedGameObject : selectedGameObjects)
                                {
                                    undoManager.TrackGameObjectHierarchyPlacement(selectedGameObject);
                                }
                                gizmoTransformTransactionActive = undoManager.HasActiveTransaction();
                                gizmoTransformTransactionSelection = selectedGameObjectUUIDs;
                            }
                        }

                        for (GameObject* selectedGameObject : selectedGameObjects)
                        {
                            glm::mat4 worldMatrix = selectedGameObject->GetWorldMatrix();

                            // handle translation and rotation
                            auto afterTR = deltaTR * worldMatrix;
                            selectedGameObject->SetWorldMatrix(afterTR);

                            // handle scale
                            auto afterS = baseModel * deltaS * baseModelInv * selectedGameObject->GetWorldMatrix();
                            selectedGameObject->SetWorldMatrix(afterS);
                        }
                    }
                }

                if (gizmoTransformTransactionActive && !ImGuizmo::IsUsing())
                {
                    EditorState::GetUndoManager().EndTransaction();
                    gizmoTransformTransactionActive = false;
                    gizmoTransformTransactionSelection.clear();
                }

                // Camera Gizmo
                float distance = 5.0f;
                if (go)
                {
                    distance = glm::length(go->GetPosition() - mainCam->GetGameObject()->GetPosition());
                }

                // inverse inverse ... because ImGuizmo is using right hand coordinate system :(
                const glm::mat4& oriView = mainCam->GetViewMatrix();
                auto view = oriView;
                float4x4 invView = glm::inverse(view);
                invView[2] = -invView[2];
                view = glm::inverse(invView);

                bool disableDragging = !activeViewGizmos;
                auto viewBefore = view;
                ImGuizmo::ViewManipulate(
                    &view[0][0],
                    distance,
                    viewManipulateRectMin,
                    ImVec2(100, 100),
                    0x10101010,
                    disableDragging
                );

                if (cameraDirty || viewBefore != view)
                {
                    invView = glm::inverse(view);
                    invView[2] = -invView[2];
                    view = glm::inverse(invView);
                    mainCam->SetViewMatrix(view);
                    auto angles = ComputeEditorCameraAnglesFromForward(mainCam->GetForward(), cameraLookAroundContext.yaw);
                    cameraLookAroundContext.yaw = angles.yaw;
                    cameraLookAroundContext.pitch = angles.pitch;
                    cameraLookAroundContext.anglesInitialized = true;
                }

                // Projection mode toggle under the view gizmo
                {
                    bool isOrtho = mainCam->GetProjectionMode() == Camera::ProjectionMode::Orthographic;
                    ImVec2 prev = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(ImVec2(viewManipulateRectMin.x, viewManipulateRectMin.y + 100.0f + 4.0f));
                    if (ImGui::Checkbox("Ortho", &isOrtho))
                    {
                        mainCam->SetProjectionMode(
                            isOrtho ? Camera::ProjectionMode::Orthographic : Camera::ProjectionMode::Perspective
                        );
                    }

                    // If orthographic, show size slider just below the checkbox
                    if (isOrtho)
                    {
                        float y = viewManipulateRectMin.y + 100.0f + 4.0f + ImGui::GetFrameHeightWithSpacing();
                        ImGui::SetCursorScreenPos(ImVec2(viewManipulateRectMin.x, y));
                        float orthoSize = mainCam->GetOrthographicSize();
                        ImGui::PushItemWidth(100.0f);
                        if (ImGui::SliderFloat("Size", &orthoSize, 0.1f, 20.0f, "%.2f"))
                        {
                            mainCam->SetOrthographicSize(orthoSize);
                        }
                        ImGui::PopItemWidth();
                    }

                    ImGui::SetCursorScreenPos(prev);
                }
            }
        }

        auto logs = HudDebug::Singleton().FlushLogs();
        for (auto log : logs)
        {
            ImGui::Text("%s", log.data());
        }
    }

    ImGui::End();

    ENGINE_END_PROFILE;
    return open;
}

void SceneEditor::EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& deltaMatrix, glm::mat4 proj)
{

    if (ImGui::IsWindowFocused() && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W))
            currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            currentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) // r Key
            currentGizmoOperation = ImGuizmo::SCALE;
    }

    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE))
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE))
        currentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE))
        currentGizmoOperation = ImGuizmo::SCALE;

    if (currentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL))
            currentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD))
            currentGizmoMode = ImGuizmo::WORLD;
    }
    else
    {
        currentGizmoMode = ImGuizmo::LOCAL;
    }

    ImGui::Checkbox("Snap to xy", &gameObjectConfigs.useSnap);
    glm::mat4 view = camera.GetViewMatrix();

    float4x4 reverzedZFix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 1};
    float4x4 imGuizmoProj = reverzedZFix * proj;

    ImGuizmo::Manipulate(
        &view[0][0],
        &imGuizmoProj[0][0],
        currentGizmoOperation,
        currentGizmoMode,
        &matrix[0][0],
        &deltaMatrix[0][0],
        gameObjectConfigs.useSnap ? &gameObjectConfigs.snap[0] : nullptr
    );
}

void SceneEditor::ChangeGameScreenResolution(glm::ivec2 resolution)
{
    if (resolution.x > 0 && resolution.y > 0)
        CreateRenderData(resolution.x, resolution.y);
}

void SceneEditor::FocusOnObject(Camera& cam, GameObject& gameObject)
{
    auto ViewSpaceMinMaxTest = [](const float4x4& viewMatrix, const AABB& aabb, float3& outMinAABB, float3& outMaxAABB)
    {
        auto minv = viewMatrix * glm::vec4(aabb.min, 1.0f);
        auto maxv = viewMatrix * glm::vec4(aabb.max, 1.0f);

        auto centerV = (maxv + minv) / 2.0f;

        // move to camera center
        minv.x -= centerV.x;
        minv.y -= centerV.y;
        minv.z -= centerV.z;
        maxv.x -= centerV.x;
        maxv.y -= centerV.y;
        maxv.z -= centerV.z;

        glm::vec3 v000 = {minv.x, minv.y, minv.z};
        glm::vec3 v100 = {maxv.x, minv.y, minv.z};
        glm::vec3 v010 = {minv.x, maxv.y, minv.z};
        glm::vec3 v001 = {minv.x, minv.y, maxv.z};

        glm::vec3 v110 = {maxv.x, maxv.y, minv.z};
        glm::vec3 v011 = {minv.x, maxv.y, maxv.z};
        glm::vec3 v101 = {maxv.x, minv.y, maxv.z};
        glm::vec3 v111 = {maxv.x, maxv.y, maxv.z};

        glm::vec3 minAABBV0 = glm::min(
            v000,
            glm::min(v100, glm::min(v010, glm::min(v001, glm::min(v110, glm::min(v011, glm::min(v101, v111))))))
        );
        glm::vec3 maxAABBV0 = glm::max(
            v000,
            glm::max(v100, glm::max(v010, glm::max(v001, glm::max(v110, glm::max(v011, glm::max(v101, v111))))))
        );

        outMinAABB = glm::min(outMinAABB, minAABBV0);
        outMaxAABB = glm::max(outMaxAABB, maxAABBV0);
    };

    auto meshRenderers = gameObject.GetComponentsInChildren<MeshRenderer>();
    auto viewMatrix = cam.GetViewMatrix();
    glm::vec3 minAABBV =
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    glm::vec3 maxAABBV =
        {std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};

    float3 center(0);
    if (!meshRenderers.empty())
    {
        float avgCenterCount = 0;

        for (auto m : meshRenderers)
        {
            auto aabb = m->GetAABB();
            center += aabb.GetCenter();
            avgCenterCount += 1;
            ViewSpaceMinMaxTest(viewMatrix, aabb, minAABBV, maxAABBV);
            avgCenterCount = 1;
        }

        if (avgCenterCount != 0)
        {
            center /= avgCenterCount;
        }
    }
    else
    {
        center = gameObject.GetPosition();
        auto fakeMax = viewMatrix * float4(center + 0.25f, 1.0f);
        auto fakeMin = viewMatrix * float4(center - 0.25f, 1.0f);

        ViewSpaceMinMaxTest(viewMatrix, {fakeMin, fakeMax}, minAABBV, maxAABBV);
    }

    float maxSide = glm::max(glm::abs(minAABBV.x), glm::max(glm::abs(minAABBV.y), glm::max(glm::abs(maxAABBV.x), glm::abs(maxAABBV.y))));
    maxSide = glm::max(maxSide, glm::max(glm::abs(minAABBV.z), glm::abs(maxAABBV.z)));

    float fov = cam.GetFoV();
    float distance = maxSide / fov;

    glm::vec3 forward = cam.GetForward();
    cam.GetGameObject()->SetPosition(center - forward * distance * 1.3f);
}

Camera* SceneEditor::GetCurrentlyActiveCamera()
{
    return editorCamera.Get();
}

void SceneEditor::ResetGizmoState()
{
    gizmoManager->ResetState();
}

void SceneEditor::RenderObjectToOutlineRT(Gfx::CommandBuffer& cmd, GameObject*& go, int colorType)
{
    if (go)
    {
        auto mrs = go->GetComponentsInChildren<MeshRenderer>();
        Rendering::DrawList drawList;
        for (MeshRenderer* meshRenderer : mrs)
        {
            if (meshRenderer)
            {
                drawList.Add(*meshRenderer);
            }
        }

        cmd.BindResource(0, renderPipeline->GetPerSceneGPUResource());
        cmd.BindShaderProgram(
            outlineRawColorPassShader->GetShaderProgram(),
            outlineRawColorPassShader->GetShaderProgram()->GetDefaultShaderConfig()
        );
        for (auto& draw : drawList)
        {
            cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
            cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
            struct
            {
                float4x4 model;
                float4 color;
                float4 padding0;
                float4 padding1;
                float4 padding2;
            } ps;
            memcpy(&ps.model, draw.GetPushConstant().data(), sizeof(float4x4));
            ps.color = float4(colorType);

            cmd.SetPushConstant(outlineRawColorPassShader->GetShaderProgram(), (void*)&ps);
            cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
        }
    }
}
void SceneEditor::DrawOutlineAndGizmos(Gfx::CommandBuffer& cmd, Gfx::Image* sceneImage, const Gfx::ImageIdentifier* gameImage, const Gfx::ImageIdentifier* gameDepthImage)
{
    auto hightedGameObject = editorContext->GetHighlightedGameObject();
    auto selectedObjects = EditorState::GetSelectedObjects();
    bool hasGameObjectSelected = false;

    // selection outline src pass
    Gfx::RenderImageDescriptor desc{
        sceneImage->GetDescription().width,
        sceneImage->GetDescription().height,
        sceneImage->GetDescription().format
    };
    cmd.AllocateAttachment(outlineSrcRT, desc);
    outlineSrcPass.SetAttachment(0, outlineSrcRT);
    Gfx::ClearValue outlineSrcPassClears[] = {{0, 0, 0, 0}};
    cmd.BeginRenderPass(outlineSrcPass, outlineSrcPassClears);
    for (auto& selected : selectedObjects)
    {
        GameObject* go = dynamic_cast<GameObject*>(selected.Get());
        if (showSelectionOutline)
        {
            hasGameObjectSelected = true;
            RenderObjectToOutlineRT(cmd, go, 0);
        }

        // we don't want to highlight the same object twice
        if (go == hightedGameObject)
            hightedGameObject = nullptr;
    }

    if (showHoverHighlightOutline && hightedGameObject)
    {
        hasGameObjectSelected = true;
        RenderObjectToOutlineRT(cmd, hightedGameObject, 1);
    }

    cmd.EndRenderPass();

    // draw outline and gizmos
    {
        gameImagePass.SetAttachment(0, *gameImage);
        if (gameDepthImage)
            gameImagePass.SetAttachment(1, *gameDepthImage);

        Gfx::ClearValue gameImagePassClears[] = {{0, 0, 0, 0}, {1, 0}};
        cmd.BeginRenderPass(gameImagePass, gameImagePassClears);
        if (hasGameObjectSelected)
        {
            outlineGPUResource->SetImage("mainTex", GetGfxDriver()->GetImageFromRenderGraph(outlineSrcRT));
            cmd.BindResource(0, outlineGPUResource.get());
            cmd.BindShaderProgram(
                outlineFullScreenPassShader->GetShaderProgram(),
                outlineFullScreenPassShader->GetShaderProgram()->GetDefaultShaderConfig()
            );
            cmd.Draw(6, 1, 0, 0);
        }

        // draw grid
        if (editorWorldSpaceGrid.show)
        {
            auto activeCamera = GetCurrentlyActiveCamera();
            if (activeCamera == editorCamera.Get())
            {
                glm::vec3 pos = glm::floor(activeCamera->GetGameObject()->GetPosition());
                pos.y = 0;
                gizmoManager->DrawMesh(
                    gridGizmo,
                    editorWorldSpaceGrid.plane,
                    0,
                    editorWorldSpaceGrid.gridShader,
                    glm::scale(glm::translate(glm::mat4(1), pos), editorWorldSpaceGrid.scale)
                );
            }
        }

        if (showGizmos)
        {
            gizmoManager->Render(editorCamera, renderPipeline->GetPerSceneGPUResource(), cmd);
        }

        if (activeTool)
        {
            cmd.BindResource(0, renderPipeline->GetPerSceneGPUResource());
            activeTool->OnDraw(cmd);
        }

        gizmoManager->ClearInactiveGizmos();
        if (showGizmos)
        {
            Gizmos::DispatchAllDiszmos(cmd, renderPipeline->GetPerSceneGPUResource());
        }
        Gizmos::ClearAllRegisteredGizmos();
        cmd.EndRenderPass();
    }
}

std::vector<GameObject*> SceneEditor::CollectGameObjectsInRect(
    Scene& scene, Camera& camera, glm::vec2 uvMin, glm::vec2 uvMax, float imageWidth, float imageHeight
)
{
    std::vector<GameObject*> result;

    float aspect = (imageWidth > 0 && imageHeight > 0) ? imageWidth / imageHeight : 1.0f;
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetAndUpdateProjectionMatrix(aspect);
    glm::mat4 vp = proj * view;

    glm::vec2 rectMin = glm::min(uvMin, uvMax);
    glm::vec2 rectMax = glm::max(uvMin, uvMax);
    SelectionRectNDC rect{
        rectMin.x * 2.0f - 1.0f,
        rectMax.x * 2.0f - 1.0f,
        rectMin.y * 2.0f - 1.0f,
        rectMax.y * 2.0f - 1.0f,
    };

    scene.ForEachGameObject([&](GameObject* go)
    {
        auto mr = go->GetComponent<MeshRenderer>();
        if (!mr || !go->IsActiveInScene())
            return;

        if (!ProjectedAABBOverlapsSelectionRect(mr->GetAABB(), vp, rect))
            return;

        if (ProjectedAABBInsideSelectionRect(mr->GetAABB(), vp, rect))
        {
            result.push_back(go);
            return;
        }

        glm::mat4 model = go->GetWorldMatrix();
        glm::mat4 mvp = vp * model;

        bool hasAnyTriangle = false;
        bool allInside = true;

        for (ObjPtr<Mesh>& meshPtr : mr->GetMeshes())
        {
            Mesh* mesh = meshPtr.Get();
            if (!mesh)
                continue;

            for (const Submesh& submesh : mesh->GetSubmeshes())
            {
                if (!ProjectedAABBOverlapsSelectionRect(submesh.GetAABB(), mvp, rect))
                    continue;

                if (ProjectedAABBInsideSelectionRect(submesh.GetAABB(), mvp, rect))
                    continue;

                const auto& indices = submesh.GetIndices();
                const auto& positions = submesh.GetPositions();
                for (int i = 0; i + 2 < submesh.GetIndexCount(); i += 3)
                {
                    uint32_t i0 = indices[i];
                    uint32_t i1 = indices[i + 1];
                    uint32_t i2 = indices[i + 2];
                    if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
                        continue;

                    hasAnyTriangle = true;

                    if (!TriangleFullyInsideSelectionRect(positions[i0], positions[i1], positions[i2], mvp, rect))
                    {
                        allInside = false;
                        break;
                    }
                }
                if (!allInside)
                    break;
            }
            if (!allInside)
                break;
        }

        if (allInside && hasAnyTriangle)
            result.push_back(go);
    });

    return result;
}

} // namespace Editor
