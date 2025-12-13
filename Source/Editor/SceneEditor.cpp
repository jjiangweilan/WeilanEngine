#include "SceneEditor.hpp"

#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/DebugOptions.hpp"
#include "Core/EngineState.hpp"
#include "Core/Gizmo.hpp"
#include "Core/SystemInfo.hpp"
#include "Core/Time.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/HudDebug.hpp"
#include "GameEditor.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Math.hpp"
#include "Physics/JoltDebugRenderer.hpp"
#include "PickObjectFromGameView.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{

SceneEditor::SceneEditor() {}
SceneEditor::~SceneEditor() {}

void SceneEditor::Deinit() {}

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

    if (GameEditor::instance->editorConfig.contains("editorCamera"))
    {
        auto& camJson = GameEditor::instance->editorConfig["editorCamera"];
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

    outlineRawColorPassShader = ShaderLibrary::GetShader(Shaders::PostProcess_OutlineRawColorPass);
    outlineFullScreenPassShader = ShaderLibrary::GetShader(Shaders::PostProcess_OutlineFullScreenPass);

    editorWorldSpaceGrid.plane =
        static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.fbx"))
            ->GetMeshes()[0]
            .get();
    editorWorldSpaceGrid.gridShader = ShaderLibrary::GetShader(Shaders::PlaneGrid);

    ChangeGameScreenResolution({256, 256});
}

void SceneEditor::EditorCameraWalkAround(Camera& editorCamera, float& editorCameraSpeed)
{
    // If the ImGui window is not hovered, exit the function
    if (!ImGui::IsWindowHovered())
        return;

    // Get the mouse delta for the right mouse button
    auto mouseDelta = mouseTrack.GetMouseDelta(ImGuiMouseButton_Right);
    bool isMouseRightButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    bool isMiddleButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    if (isMouseRightButtonDown)
    {
        if (!cameraLookAroundContext.isActive)
        {
            cameraLookAroundContext.isActive = true;
            cameraLookAroundContext.startPos = editorCamera.GetGameObject()->GetPosition();
        }

        // Retrieve the game object associated with the editor camera
        auto go = editorCamera.GetGameObject();
        auto pos = go->GetPosition();
        glm::mat4 model = go->GetWorldMatrix();
        glm::vec3 right = glm::normalize(model[0]);    // Right direction vector
        glm::vec3 up = glm::normalize(model[1]);       // Up direction vector
        glm::vec3 forward = -glm::normalize(model[2]); // Forward direction vector

        // Adjust camera speed if the Alt key is held down
        float mouseWheel = ImGui::GetIO().MouseWheel;
        if (isAltDown)
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
            if (mouseWheel != 0)
                spdlog::info("change editor camera speed to {}", editorCameraSpeed);
        }

        // Calculate movement speed based on delta time
        float speed = editorCameraSpeed * Time::DeltaTime();

        glm::vec3 dir = glm::vec3(0);

        // Handle movement input for the camera
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            dir += right * speed; // Move right
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            dir -= right * speed; // Move left
        }
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            dir += forward * speed; // Move forward
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            dir -= forward * speed; // Move backward
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            dir += up * speed; // Move up
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            dir -= up * speed; // Move down
        }
        // scroll the mouse wheel to zoom in and out
        if (!isAltDown && mouseWheel != 0.0f)
        {
            dir += forward * speed * mouseWheel; // Zoom in or out
        }

        // Update the position of the game object
        pos += dir;
        go->SetPosition(pos);

        // Calculate camera rotation based on mouse movement
        auto upDown = 25 * glm::radians(mouseDelta.y) * Time::DeltaTime();
        auto leftRight = 25 * glm::radians(mouseDelta.x) * Time::DeltaTime();
        auto lookAtDelta = leftRight * right + upDown * up;
        go->LookAt(forward + lookAtDelta);
    }
    else if (isMiddleButtonDown)
    {
        // Handle panning movement when the middle mouse button is held down
        auto upDown = glm::radians(mouseDelta.y * 100) * Time::DeltaTime();
        auto leftRight = glm::radians(mouseDelta.x * 100) * Time::DeltaTime();

        auto go = editorCamera.GetGameObject();
        auto pos = go->GetPosition();
        pos += go->GetUp() * upDown + leftRight * go->GetRight();
        go->SetPosition(pos);
    }

    if (!isMouseRightButtonDown)
    {
        cameraLookAroundContext.isActive = false;
    }

    // Print the current position of the editor camera to the HUD
    glm::vec3 pos = editorCamera.GetGameObject()->GetPosition();
    HudDebug::Print(fmt::format("{}, {}, {}", pos.x, pos.y, pos.z));
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
    Rendering::RenderConfig renderConfig = {.drawGraphics = true, .cmdOverride = &cmd};
    renderPipeline->SetConfig(renderConfig);
    renderPipeline->Render(*scene, *editorCamera, d.resolution);
    auto gameImage = &renderPipeline->GetOutputColor();
    auto gameDepthImage = &renderPipeline->GetOutputDepth();

    auto selectedObjects = EditorState::GetSelectedObjects();
    bool hasGameObjectSelected = false;
    // selection outline src pass
    {
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
            if (go)
            {
                hasGameObjectSelected = true;
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
                    auto ps = draw.GetPushConstant();
                    cmd.SetPushConstant(draw.shader->GetShaderProgram(), (void*)&ps);
                    cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                }
            }
        }
        cmd.EndRenderPass();
    }

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

        gizmoManager->Render(editorCamera, renderPipeline->GetPerSceneGPUResource(), cmd);
        gizmoManager->ClearInactiveGizmos();
        Gizmos::DispatchAllDiszmos(cmd, renderPipeline->GetPerSceneGPUResource());
        Gizmos::ClearAllRegisteredGizmos();
        cmd.EndRenderPass();
    }

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

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Debug Draw"))
        {
            ImGui::Checkbox("Physics", &GetDebugOptions().drawPhysicsColliders);
            ImGui::Checkbox("Game Object", &GetDebugOptions().drawGameObjectDebugDraw);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Toggle Grid"))
        {
            editorWorldSpaceGrid.show = !editorWorldSpaceGrid.show;
        }
        float fovDegrees = glm::degrees(editorCamera->GetFoV());
        if (ImGui::DragFloat("Camera FoV", &fovDegrees, 0.1f, 1.0f, 179.0f))
        {
            editorCamera->SetFoV(glm::radians(fovDegrees));
        }
        ImGui::EndMenuBar();
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
                FocusOnObject(*mainCam, *go);
        }
    }

    EditorCameraWalkAround(*editorCamera, editorCameraSpeed);

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

        if (scene)
        {
            // Gizmo
            for (auto g : scene->GetAllGameObjects())
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
                GizmoBase::ClearActiveCarrier();
            }
        }

        bool anyItemHovered = ImGui::IsAnyItemHovered();
        auto selected = EditorState::GetMainSelectedObject();
        if (!anyItemHovered && (selected == nullptr || !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) && !hoveringViewGizmo && !gizmoManager->AnyGizmoActive())
        {
            // pick a GameObject trough ray
            if (isMouseClicked && isGameViewHovered && ImGui::IsWindowFocused())
            {
                auto mainCam = GetCurrentlyActiveCamera();
                if (mainCam != nullptr)
                {
                    using Intersected = PickGameObjectFromScene::Intersected;
                    Ray ray = mainCam->ScreenUVToWorldSpaceRay(screenUV);

                    std::vector<Intersected> intersected;
                    intersected.reserve(32);
                    std::vector<GameObject*> results;
                    Gizmos::PickGizmos(ray, results);
                    if (results.empty())
                    {
                        if (auto scene = SceneManager::GetActiveScene())
                        {
                            auto sceneIntersected = PickGameObjectFromScene()(*scene, ray, screenUV);
                            intersected.insert(intersected.end(), sceneIntersected.begin(), sceneIntersected.end());
                        }
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
                        // if picked is already selected, deselect it, otherwise select it
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
                            // else
                            // {
                            //     EditorState::SelectObject(picked, false);
                            // }
                        }
                    }
                    else
                    {
                        EditorState::SelectObject(nullptr);
                    }
                }
            }
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
                    auto baseModel = go->GetWorldMatrix();

                    glm::vec3 avgPos = glm::vec3(0);

                    for (auto& s : selectedObjects)
                    {
                        auto go = static_cast<GameObject*>(s.Get());
                        avgPos += go->GetPosition();
                    }
                    avgPos /= selectedObjects.size();
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
                        for (auto& s : selectedObjects)
                        {
                            auto go = static_cast<GameObject*>(s.Get());
                            glm::mat4 worldMatrix = go->GetWorldMatrix();

                            // handle translation and rotation
                            auto afterTR = deltaTR * worldMatrix;
                            go->SetWorldMatrix(afterTR);

                            // handle scale
                            auto afterS = baseModel * deltaS * baseModelInv * go->GetWorldMatrix();
                            go->SetWorldMatrix(afterS);
                        }
                    }
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
                ImGuizmo::ViewManipulate(
                    &view[0][0],
                    distance,
                    viewManipulateRectMin,
                    ImVec2(100, 100),
                    0x10101010,
                    disableDragging
                );

                invView = glm::inverse(view);
                invView[2] = -invView[2];
                view = glm::inverse(invView);
                mainCam->SetViewMatrix(view);

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

    glm::vec3 center = gameObject.GetPosition();
    auto meshRenderers = gameObject.GetComponentsInChildren<MeshRenderer>();
    auto viewMatrix = cam.GetViewMatrix();
    glm::vec3 minAABBV =
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    glm::vec3 maxAABBV =
        {std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};

    if (!meshRenderers.empty())
    {
        for (auto m : meshRenderers)
        {
            auto aabb = m->GetAABB();
            ViewSpaceMinMaxTest(viewMatrix, aabb, minAABBV, maxAABBV);
        }
    }
    else
    {
        auto fakeMax = viewMatrix * float4(center + 0.25f, 1.0f);
        auto fakeMin = viewMatrix * float4(center - 0.25f, 1.0f);

        ViewSpaceMinMaxTest(viewMatrix, {fakeMin, fakeMax}, minAABBV, maxAABBV);
    }

    float maxSide = glm::max(
        glm::abs(minAABBV.x),
        glm::max(glm::abs(minAABBV.y), glm::max(glm::abs(maxAABBV.x), glm::abs(maxAABBV.y)))
    );
    maxSide = glm::max(maxSide, glm::max(glm::abs(minAABBV.z), glm::abs(maxAABBV.z)));

    float fov = cam.GetFoV();
    float distance = maxSide / fov;

    glm::vec3 forward = cam.GetForward();
    cam.GetGameObject()->SetPosition(center - forward * distance);
}

Camera* SceneEditor::GetCurrentlyActiveCamera()
{
    return editorCamera.Get();
}

void SceneEditor::ResetGizmoState()
{
    gizmoManager->ResetState();
}
} // namespace Editor
