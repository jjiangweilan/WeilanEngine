#include "GameView.hpp"

#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/EngineState.hpp"
#include "Core/Gizmo.hpp"
#include "Core/SystemInfo.hpp"
#include "Core/Time.hpp"
#include "Editor/HudDebug.hpp"
#include "EditorState.hpp"
#include "GameEditor.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Math.hpp"
#include "Physics/JoltDebugRenderer.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include "Tools/PickObjectFromGameView.hpp"

namespace Editor
{

struct GameView::PlayTheGame
{
    PlayTheGame() {}
    ObjPtr<Scene> sceneCopy;
    std::filesystem::path originalScenePath;
    bool played = false;

    void Play(GameView* gameView)
    {
        if (!played)
        {
            played = true;
            auto& scene = *EditorState::activeScene;

            AssetDatabase::Singleton()->SaveAsset(scene);
            originalScenePath = AssetDatabase::Singleton()->GetAssetPath(scene.GetUUID());
            AssetDatabase::Singleton()->UnloadAsset(scene);
            sceneCopy = AssetDatabase::Singleton()->LoadAsset(originalScenePath);
            sceneCopy->SetName("scene copy");
            sceneCopy->SetFlags(AssetStateFlags::DontSave);
            gameView->gameCamera = sceneCopy->GetMainCamera();
            EditorState::activeScene = sceneCopy;
            EditorState::gameLoop->SetScene(*sceneCopy);
            gameView->editorCameraGO->SetScene(sceneCopy);
            EngineState::GetSingleton().isPlaying = true;
            EditorState::gameLoop->Play();
        }
    }

    void Stop(GameView* gameView)
    {
        if (played)
        {
            played = false;
            // stop execution of the loop
            EditorState::gameLoop->Stop();

            // resume editor state
            EngineState::GetSingleton().isPlaying = false;
            AssetDatabase::Singleton()->UnloadAsset(*sceneCopy);
            auto ori = (Scene*)AssetDatabase::Singleton()->LoadAsset(originalScenePath);
            if (ori)
            {
                gameView->editorCameraGO->SetScene(ori);
                gameView->gameCamera = ori->GetMainCamera();
                EditorState::activeScene = ori;
                EditorState::gameLoop->SetScene(*ori);
                EditorState::activeScene->SetMainCamera(gameView->editorCamera);
            }

            // destroy sceneCopy
            sceneCopy = nullptr;
        }
    }
};
GameView::GameView() {}
GameView::~GameView() {}

void GameView::Deinit()
{
    playTheGame->Stop(this);
}

void GameView::SetActiveScene(ObjPtr<Scene> scene)
{
    if (scene)
    {
        gameCamera = EditorState::activeScene->GetMainCamera();
        editorCamera->GetGameObject()->SetScene(EditorState::activeScene);
        EditorState::activeScene->SetMainCamera(editorCamera);
        EditorState::gameLoop->SetScene(*EditorState::activeScene);
    }
}

void GameView::Init()
{
    editorCameraGO = std::make_unique<GameObject>();
    editorCameraGO->SetName("editor camera");
    editorCamera = editorCameraGO->AddComponent<Camera>();
    playTheGame = std::make_unique<PlayTheGame>();
    outlineGPUResource = GetGfxDriver()->CreateShaderResource();
    const char* editorFinalColorBlitShaderKeyword[] = {"_ResetAlpha"};
    editorFinalColorBlitShader = ShaderLibrary::GetShader(
        "Blit",
        ShaderLibrary::QueryShaderFeatures("Blit").GetPermutation(editorFinalColorBlitShaderKeyword)
    );
    editorFinalColorBlitMaterial = std::make_unique<Material>();
    editorFinalColorBlitMaterial->SetShader(editorFinalColorBlitShader);

    Gfx::RG::SubpassAttachment color = {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::RG::SubpassAttachment colorVec[] = {color};
    editorFinalColorBlitPass.SetSubpass(0, colorVec);

    // setup camera state
    if (EditorState::activeScene)
    {
        SetActiveScene(EditorState::activeScene);
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

    outlineRawColorPassShader = ShaderLibrary::GetShader(ShaderLibrary::PostProcess_OutlineRawColorPass);
    outlineFullScreenPassShader = ShaderLibrary::GetShader(ShaderLibrary::PostProcess_OutlineFullScreenPass);

    editorWorldSpaceGrid.plane =
        static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.fbx"))
            ->GetMeshes()[0]
            .get();
    editorWorldSpaceGrid.gridShader = ShaderLibrary::GetShader(ShaderLibrary::PlaneGrid);

    ChangeGameScreenResolution({256, 256});
}

void GameView::EditorCameraWalkAround(Camera& editorCamera, float& editorCameraSpeed)
{
    if (!ImGui::IsWindowHovered())
        return;

    static ImVec2 lastMouseDelta = ImVec2(0, 0);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        auto go = editorCamera.GetGameObject();
        auto pos = go->GetPosition();
        glm::mat4 model = go->GetWorldMatrix();
        glm::vec3 right = glm::normalize(model[0]);
        glm::vec3 up = glm::normalize(model[1]);
        glm::vec3 forward = -glm::normalize(model[2]);

        if (isAltDown)
        {
            float change = ImGui::GetIO().MouseWheel;
            editorCameraSpeed += change;
            if (change != 0)
                spdlog::info("change editor camera speed to {}", editorCameraSpeed);
        }
        float speed = editorCameraSpeed * Time::DeltaTime();
        glm::vec3 dir = glm::vec3(0);
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            dir += right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            dir -= right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            dir += forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            dir -= forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            dir += up * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            dir -= up * speed;
        }
        pos += dir;
        go->SetPosition(pos);

        auto mouseLastClickDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0);
        glm::vec2 mouseDelta = {mouseLastClickDelta.x - lastMouseDelta.x, mouseLastClickDelta.y - lastMouseDelta.y};
        mouseDelta.y = -mouseDelta.y;
        lastMouseDelta = mouseLastClickDelta;
        auto upDown = glm::radians(mouseDelta.y * 50) * Time::DeltaTime();
        auto leftRight = glm::radians(mouseDelta.x * 50) * Time::DeltaTime();

        auto eye = go->GetPosition();
        auto lookAtDelta = leftRight * right + upDown * up;
        auto final = glm::lookAt(eye, eye + (forward + lookAtDelta), glm::vec3(0, 1, 0));
        final = glm::inverse(final);
        go->SetWorldMatrix(final);
    }
    else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        auto mouseLastClickDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle, 0);
        glm::vec2 mouseDelta = {mouseLastClickDelta.x - lastMouseDelta.x, mouseLastClickDelta.y - lastMouseDelta.y};
        mouseDelta.y = -mouseDelta.y;
        lastMouseDelta = mouseLastClickDelta;
        auto upDown = glm::radians(mouseDelta.y * 100) * Time::DeltaTime();
        auto leftRight = glm::radians(mouseDelta.x * 100) * Time::DeltaTime();

        auto go = editorCamera.GetGameObject();
        auto pos = go->GetPosition();
        pos += go->GetUp() * upDown + leftRight * go->GetRight();
        go->SetPosition(pos);
    }
    else
    {
        lastMouseDelta = ImVec2(0, 0);
    }

    glm::vec3 pos = editorCamera.GetGameObject()->GetPosition();
    HudDebug::Singleton().Print(fmt::format("{}, {}, {}", pos.x, pos.y, pos.z));
}

void GameView::CreateRenderData(uint32_t width, uint32_t height)
{
    pendingDeleteSceneImages.push_back({std::move(sceneImage), 0});

    sceneImage = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R8G8B8A8_SRGB),
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );

    SystemInfo::Singleton().SetScreenSize(width, height);
    editorCamera->SetProjectionMatrix(glm::radians(60.0f), width / (float)height, 0.01f, 1000.f);
}

void GameView::Render(
    Gfx::CommandBuffer& cmd, const Gfx::RG::ImageIdentifier* gameImage, const Gfx::RG::ImageIdentifier* gameDepthImage
)
{
    glm::float4 renderPassLabelColor{0.4, 0.5, 0.13, 1.0};
    if (gameImage && gameDepthImage)
    {
        cmd.BeginLabel("Game View", &renderPassLabelColor[0]);
        auto selectedObjects = EditorState::GetSelectedObjects();
        bool hasGameObjectSelected = false;
        // selection outline src pass
        {

            Gfx::RG::ImageDescription desc{
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
                    Rendering::FrameGraph::DrawList drawList;
                    for (MeshRenderer* meshRenderer : mrs)
                    {
                        if (meshRenderer)
                        {
                            drawList.Add(*meshRenderer);
                        }
                    }

                    cmd.BindResource(0, EditorState::gameLoop->GetRenderPipeline().GetPerSceneGPUResource());
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
                if (activeCamera == editorCamera)
                {
                    glm::vec3 pos = glm::floor(activeCamera->GetGameObject()->GetPosition());
                    pos.y = 0;
                    Gizmos::DrawMesh(
                        *editorWorldSpaceGrid.plane,
                        0,
                        editorWorldSpaceGrid.gridShader,
                        glm::scale(glm::translate(glm::mat4(1), pos), editorWorldSpaceGrid.scale)
                    );
                }
            }

            Gizmos::DispatchAllDiszmos(cmd, EditorState::gameLoop->GetRenderPipeline().GetPerSceneGPUResource());
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
}

bool GameView::Tick()
{
    for (auto& p : pendingDeleteSceneImages)
    {
        p.frameCount += 1;
    }
    pendingDeleteSceneImages.remove_if([](PendingDelete& p) { return p.frameCount > 5; });

    bool open = true;

    ImGui::Begin("Game View", &open, ImGuiWindowFlags_MenuBar);

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

    const char* menuSelected = "";
    Scene* scene = EditorState::activeScene;
    if (ImGui::BeginMenuBar())
    {
        const char* toggleViewCamera = "Toggle View Camera: On";
        if (!useViewCamera)
            toggleViewCamera = "Toggle View Camera: Off";
        if (ImGui::MenuItem(toggleViewCamera))
        {
            useViewCamera = !useViewCamera;
            if (useViewCamera)
            {
                EditorState::activeScene->SetMainCamera(editorCamera);
            }
            else
            {
                // let scene search a main camera
                EditorState::activeScene->SetMainCamera(nullptr);
            }

            Input::GetSingleton().SetGameplayInput(!useViewCamera);
        }
        if (ImGui::MenuItem("Resolution"))
        {
            menuSelected = "Change Resolution";
        }
        if (ImGui::MenuItem("Auto Resize"))
        {
            menuSelected = "Auto Resize";
        }
        if (playTheGame->played && ImGui::MenuItem("Stop"))
        {
            menuSelected = "Stop";
        }
        if (!playTheGame->played && ImGui::MenuItem("Play"))
        {
            menuSelected = "Play";
        }
        if (ImGui::MenuItem("Overlay"))
        {
            menuSelected = "Overlay";
        }
        if (ImGui::MenuItem("Physics Debug Draw"))
        {
            JoltDebugRenderer::GetDrawAll() = !JoltDebugRenderer::GetDrawAll();
        }
        if (ImGui::MenuItem("Toggle Grid"))
        {
            editorWorldSpaceGrid.show = !editorWorldSpaceGrid.show;
        }
        ImGui::EndMenuBar();
    }

    if (strcmp(menuSelected, "Change Resolution") == 0)
    {
        ImGui::OpenPopup("Change Resolution");
        uint32_t width = 1920;
        uint32_t height = 1080;

        if (sceneImage)
        {
            width = sceneImage->GetDescription().width;
            height = sceneImage->GetDescription().height;
        }

        d.resolution = {width, height};
    }
    else if (strcmp(menuSelected, "Auto Resize") == 0)
    {
        int width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        int height = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
        ChangeGameScreenResolution({width, height});
    }
    else if (strcmp(menuSelected, "Play") == 0)
    {
        playTheGame->Play(this);
        scene = EditorState::activeScene;
    }
    else if (strcmp(menuSelected, "Pause") == 0)
    {
        EditorState::gameLoop->Stop();
    }
    else if (strcmp(menuSelected, "Stop") == 0)
    {
        playTheGame->Stop(this);
        scene = EditorState::activeScene;
    }
    else if (strcmp(menuSelected, "Overlay") == 0)
    {}

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

    if (ImGui::BeginPopup("Change Resolution"))
    {
        ImGui::InputInt2("Resolution", (int*)&d.resolution);
        if (ImGui::Button("Confirm"))
        {
            ChangeGameScreenResolution(d.resolution);
        }
        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C) && ImGui::IsWindowFocused())
    {
        if (Scene* scene = EditorState::activeScene)
        {
            if (GameObject* selected = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject()))
            {
                EditorState::SelectObject(scene->CopyGameObject(*selected)->GetSRef());
            }
        }
    }

    if (ImGui::IsKeyReleased(ImGuiKey_F))
    {
        if (GameObject* go = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject()))
        {
            if (auto mainCam = GetCurrentlyActiveCamera())
                FocusOnObject(*mainCam, *go);
        }
    }

    if (useViewCamera)
        EditorCameraWalkAround(*editorCamera, editorCameraSpeed);

    // create scene color if it's null or if the window size is changed
    const auto contentMax = ImGui::GetWindowContentRegionMax();
    const auto contentMin = ImGui::GetWindowContentRegionMin();
    const float contentWidth = contentMax.x - contentMin.x;
    const float contentHeight = contentMax.y - contentMin.y;

    if (sceneImage)
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

        if (scene)
        {
            // Gizmo
            for (auto g : scene->GetAllGameObjects())
            {
                GizmoBase::SetActiveCarrier(g);
                for (auto& c : g->GetComponents())
                {
                    c->OnDrawGizmos();
                }
                GizmoBase::ClearActiveCarrier();
            }
        }

        auto contentRegionWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        auto cursorPos = ImGui::GetCursorPos();
        cursorPos.x = cursorPos.x + contentRegionWidth / 2.0f - imageWidth / 2.0f;
        ImGui::SetCursorPos(cursorPos);

        auto imagePos = ImGui::GetCursorPos();
        ImGui::Image(&sceneImage->GetDefaultImageView(), {imageWidth, imageHeight});
        bool isGameViewHovered = ImGui::IsItemHovered();

        auto windowPos = ImGui::GetWindowPos();
        if (!ImGuizmo::IsUsing())
        {
            // pick a GameObject trough ray
            if (useViewCamera && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isGameViewHovered &&
                ImGui::IsWindowFocused())
            {
                auto mousePos = ImGui::GetMousePos();
                glm::vec2 mouseContentPos{mousePos.x - windowPos.x - imagePos.x, mousePos.y - windowPos.y - imagePos.y};

                glm::vec2 screenUV = mouseContentPos / glm::vec2{imageWidth, imageHeight};

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
                        if (EditorState::activeScene)
                        {
                            auto sceneIntersected = PickGameObjectFromScene()(*EditorState::activeScene, ray, screenUV);
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
                            [picked](SRef<Object> o) { return o.Get() == picked; }
                        );
                        if (findIter == selectedObjects.end())
                        {
                            bool multiSelect = ImGui::IsKeyDown(ImGuiKey_LeftShift);
                            EditorState::SelectObject(picked->GetSRef(), multiSelect);
                        }
                        else
                        {
                            if (isAltDown)
                            {
                                EditorState::DeselectObject(picked);
                            }
                            else
                            {
                                EditorState::SelectObject(picked->GetSRef(), false);
                            }
                        }
                    }
                    else
                    {
                        EditorState::SelectObject(nullptr);
                    }
                }
            }
        }

        ImGui::SetCursorPos(imagePos);
        if (useViewCamera && scene != nullptr)
        {
            auto mainCam = GetCurrentlyActiveCamera();
            if (mainCam)
            {
                glm::vec4 rect = {imagePos.x + windowPos.x, imagePos.y + windowPos.y, imageWidth, imageHeight};
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetGizmoSizeClipSpace(0.2f);
                ImGuizmo::SetRect(rect.x, rect.y, rect.z, rect.w);

                glm::mat4 proj = mainCam->GetProjectionMatrix();
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
                glm::mat4 view = mainCam->GetViewMatrix();
                ImGuizmo::ViewManipulate(
                    &view[0][0],
                    distance,
                    ImVec2(rect.x + imageWidth - 105, rect.y + 105),
                    ImVec2(100, -100),
                    0x10101010
                );
                // auto world = glm::inverse(view);
                // world[2] = -world[2];
                // mainCam->GetGameObject()->SetWorldMatrix(world);
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

void GameView::EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& deltaMatrix, glm::mat4& proj)
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
    ImGuizmo::Manipulate(
        &view[0][0],
        &proj[0][0],
        currentGizmoOperation,
        currentGizmoMode,
        &matrix[0][0],
        &deltaMatrix[0][0],
        gameObjectConfigs.useSnap ? &gameObjectConfigs.snap[0] : nullptr
    );
}

void GameView::ChangeGameScreenResolution(glm::ivec2 resolution)
{
    if (resolution.x > 0 && resolution.y > 0)
        CreateRenderData(resolution.x, resolution.y);
}

void GameView::FocusOnObject(Camera& cam, GameObject& gameObject)
{
    glm::vec3 center = gameObject.GetPosition();
    auto meshRenderers = gameObject.GetComponentsInChildren<MeshRenderer>();
    auto viewMatrix = cam.GetViewMatrix();
    glm::vec3 minAABBV =
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    glm::vec3 maxAABBV =
        {std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};

    for (auto m : meshRenderers)
    {
        auto aabb = m->GetAABB();
        auto centerV = viewMatrix * glm::vec4(m->GetGameObject()->GetPosition(), 1.0);
        auto minv = viewMatrix * glm::vec4(aabb.min, 1.0f);
        auto maxv = viewMatrix * glm::vec4(aabb.max, 1.0f);

        // move to camera center
        minv.x -= centerV.x;
        minv.y -= centerV.y;
        maxv.x -= centerV.x;
        maxv.y -= centerV.y;

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

        minAABBV = glm::min(minAABBV, minAABBV0);
        maxAABBV = glm::max(maxAABBV, maxAABBV0);
    }

    float maxSide = glm::max(
        glm::abs(minAABBV.x),
        glm::max(glm::abs(minAABBV.y), glm::max(glm::abs(maxAABBV.x), glm::abs(maxAABBV.y)))
    );

    float fov = cam.GetFoV();
    float distance = maxSide / fov;

    glm::vec3 forward = cam.GetForward();
    cam.GetGameObject()->SetPosition(center + forward * distance);
}

Camera* GameView::GetCurrentlyActiveCamera()
{
    Camera* mainCam = nullptr;
    auto scene = EditorState::activeScene;
    mainCam = scene->GetMainCamera();
    return mainCam;
}
} // namespace Editor
