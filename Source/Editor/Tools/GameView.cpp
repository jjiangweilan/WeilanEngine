#include "GameView.hpp"

#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/EngineState.hpp"
#include "Core/Gizmo.hpp"
#include "Core/Time.hpp"
#include "EditorState.hpp"
#include "GameEditor.hpp"
#include "Physics/JoltDebugRenderer.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{

struct GameView::PlayTheGame
{
    PlayTheGame() {}
    std::unique_ptr<Scene> sceneCopy;
    SRef<Scene> originalScene;
    bool played = false;

    void Play(GameView* gameView)
    {
        if (!played)
        {
            played = true;
            auto& scene = *EditorState::activeScene;

            originalScene = scene.GetSRef<Scene>();
            sceneCopy = std::make_unique<Scene>();
            AssetDatabase::Singleton()->CopyThroughSerialization<JsonSerializer>(scene, *sceneCopy);
            sceneCopy->SetName("scene copy");
            gameView->gameCamera = sceneCopy->GetMainCamera();

            EditorState::activeScene = sceneCopy.get();
            EditorState::gameLoop->Play();
            EditorState::gameLoop->SetScene(*sceneCopy, *gameView->GetCurrentlyActiveCamera());
            gameView->editorCameraGO->SetScene(sceneCopy.get());

            EngineState::GetSingleton().isPlaying = true;
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
            auto ori = originalScene.Get();
            gameView->editorCameraGO->SetScene(ori);
            gameView->gameCamera = ori->GetMainCamera();
            if (ori)
            {
                EditorState::activeScene = ori;
                EditorState::gameLoop->SetScene(*ori, *gameView->GetCurrentlyActiveCamera());
            }

            // destroy sceneCopy
            sceneCopy = nullptr;
        }
    }
};
GameView::GameView() {}
GameView::~GameView() {}

struct Intersected
{
    GameObject* go;
    float distance;
};

void GameView::Init()
{
    editorCameraGO = std::make_unique<GameObject>();
    editorCameraGO->SetName("editor camera");
    editorCamera = editorCameraGO->AddComponent<Camera>();
    playTheGame = std::make_unique<PlayTheGame>();

    // setup camera state
    gameCamera = EditorState::activeScene->GetMainCamera();
    editorCamera->GetGameObject()->SetScene(EditorState::activeScene);
    if (gameCamera)
    {
        auto fg = gameCamera->GetFrameGraph();
        editorCamera->SetFrameGraph(fg);
        if (!fg->IsCompiled())
            fg->Compile();
    }
    EditorState::gameLoop->SetScene(*EditorState::activeScene, *editorCamera);

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

    outlineRawColorPassShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/OutlineRawColorPass.shad")
        );
    outlineFullScreenPassShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/OutlineFullScreenPass.shad"
        ));
}

static bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj, float& distance)
{
    auto mr = obj->GetComponent<MeshRenderer>();
    if (mr)
    {
        auto model = obj->GetModelMatrix();
        auto mesh = mr->GetMesh();
        if (mesh)
        {
            for (const Submesh& submesh : mesh->GetSubmeshes())
            {
                auto& indices = submesh.GetIndices();
                auto& positions = submesh.GetPositions();

                // I just assume binding zero is a vec3 position, this is not robust
                for (int i = 0; i < submesh.GetIndexCount(); i += 3)
                {
                    int j = i + 1;
                    int k = i + 2;

                    glm::vec3 v0, v1, v2;
                    v0 = model * glm::vec4(positions[indices[i]], 1.0);
                    v1 = model * glm::vec4(positions[indices[j]], 1.0);
                    v2 = model * glm::vec4(positions[indices[k]], 1.0);

                    glm::vec2 bary;
                    if (glm::intersectRayTriangle(ori, dir, v0, v1, v2, bary, distance))
                        return true;
                }
            }
        }
    }

    // ray is not tested, there is no mesh maybe use a box as proxy?
    return false;
}

static void PickGameObjectFromScene(const Ray& ray, glm::vec2 screenUV, std::vector<Intersected>& intersected)
{
    if (Scene* scene = EditorState::activeScene)
    {
        auto objs = scene->GetAllGameObjects();

        auto ori = ray.origin;
        auto dir = ray.direction;
        for (auto obj : objs)
        {
            if (obj == nullptr || !obj->IsEnabled())
                continue;

            float distance = std::numeric_limits<float>::max();
            if (IsRayObjectIntersect(ori, dir, obj, distance))
            {
                intersected.push_back(Intersected{obj, distance});
            }
        }
    }
}

static void EditorCameraWalkAround(Camera& editorCamera)
{
    if (!ImGui::IsWindowHovered())
        return;

    static ImVec2 lastMouseDelta = ImVec2(0, 0);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        auto go = editorCamera.GetGameObject();
        auto pos = go->GetPosition();
        glm::mat4 model = go->GetModelMatrix();
        glm::vec3 right = glm::normalize(model[0]);
        glm::vec3 up = glm::normalize(model[1]);
        glm::vec3 forward = glm::normalize(model[2]);

        float speed = 10 * Time::DeltaTime();
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
        auto final = glm::lookAt(eye, eye - (forward + lookAtDelta), glm::vec3(0, 1, 0));
        final = glm::inverse(final);
        go->SetModelMatrix(final);
    }
    else
    {
        lastMouseDelta = ImVec2(0, 0);
    }
}

void GameView::CreateRenderData(uint32_t width, uint32_t height)
{
    pendingDeleteSceneImages.push_back({std::move(sceneImage), 0});

    sceneImage = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::ImageFormat::R8G8B8A8_SRGB),
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );

    editorCamera->SetProjectionMatrix(glm::radians(60.0f), width / (float)height, 0.01f, 1000.f);
}

void GameView::Render(
    Gfx::CommandBuffer& cmd, const Gfx::RG::ImageIdentifier* gameImage, const Gfx::RG::ImageIdentifier* gameDepthImage
)
{
    if (gameImage && gameDepthImage)
    {
        // selection outline
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {1.0f, 0}};
        GameObject* go = dynamic_cast<GameObject*>(EditorState::selectedObject.Get());
        if (go)
        {
            auto mrs = go->GetComponentsInChildren<MeshRenderer>();
            Rendering::FrameGraph::DrawList drawList;
            for (MeshRenderer* meshRenderer : mrs)
            {
                if (meshRenderer)
                {
                    drawList.Add(*meshRenderer);
                }
            }

            Rect2D rect{{0, 0}, {sceneImage->GetDescription().width, sceneImage->GetDescription().height}};
            cmd.SetScissor(0, 1, &rect);
            cmd.SetViewport(
                Gfx::Viewport{0, 0, static_cast<float>(rect.extent.width), static_cast<float>(rect.extent.height), 0, 1}
            );
            Gfx::RG::ImageDescription desc{
                sceneImage->GetDescription().width,
                sceneImage->GetDescription().height,
                sceneImage->GetDescription().format
            };
            cmd.AllocateAttachment(outlineSrcRT, desc);
            outlineSrcPass.SetAttachment(0, outlineSrcRT);
            cmd.BeginRenderPass(outlineSrcPass, clears);
            cmd.BindShaderProgram(
                outlineRawColorPassShader->GetDefaultShaderProgram(),
                outlineRawColorPassShader->GetDefaultShaderConfig()
            );
            for (auto& draw : drawList)
            {
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.SetPushConstant(draw.shader->GetShaderProgram(0, 0), (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
            cmd.EndRenderPass();
        }

        gameImagePass.SetAttachment(0, *gameImage);
        if (gameDepthImage)
            gameImagePass.SetAttachment(1, *gameDepthImage);
        cmd.BeginRenderPass(gameImagePass, clears);

        if (go)
        {
            cmd.SetTexture("mainTex", outlineSrcRT);
            cmd.BindShaderProgram(
                outlineFullScreenPassShader->GetDefaultShaderProgram(),
                outlineFullScreenPassShader->GetDefaultShaderConfig()
            );
            cmd.Draw(6, 1, 0, 0);
        }

        Gizmos::DispatchAllDiszmos(cmd);
        Gizmos::ClearAllRegisteredGizmos();
        cmd.EndRenderPass();

        auto outputImage = GetGfxDriver()->GetImageFromRenderGraph(*gameImage);
        if (outputImage)
            cmd.Blit(outputImage, sceneImage.get());
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
            auto mainCam = GetCurrentlyActiveCamera();
            EditorState::gameLoop->SetScene(*EditorState::activeScene, *mainCam);
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
        ImGui::EndMenuBar();
    }

    if (strcmp(menuSelected, "Change Resolution") == 0)
    {
        ImGui::OpenPopup("Change Resolution");
        auto width = sceneImage->GetDescription().width;
        auto height = sceneImage->GetDescription().height;
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
    }
    else if (strcmp(menuSelected, "Pause") == 0)
    {
        EditorState::gameLoop->Stop();
    }
    else if (strcmp(menuSelected, "Stop") == 0)
    {
        playTheGame->Stop(this);
    }
    else if (strcmp(menuSelected, "Overlay") == 0)
    {}

    // alway match window size
    int width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    int height = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    if (firstFrame || width != sceneImage->GetDescription().width || height != sceneImage->GetDescription().height)
    {
        firstFrame = false;
        ChangeGameScreenResolution({width, height});
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

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C))
    {
        if (Scene* scene = EditorState::activeScene)
        {
            if (GameObject* selected = dynamic_cast<GameObject*>(EditorState::selectedObject.Get()))
            {
                EditorState::selectedObject = scene->CopyGameObject(*selected)->GetSRef();
            }
        }
    }

    if (ImGui::IsKeyReleased(ImGuiKey_F))
    {
        if (GameObject* go = dynamic_cast<GameObject*>(EditorState::selectedObject.Get()))
        {
            if (auto mainCam = GetCurrentlyActiveCamera())
                FocusOnObject(*mainCam, *go);
        }
    }

    if (useViewCamera)
        EditorCameraWalkAround(*editorCamera);

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

        auto windowPos = ImGui::GetWindowPos();
        if (!ImGuizmo::IsUsing())
        {
            // pick a GameObject trough ray
            if (useViewCamera && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() &&
                ImGui::IsWindowFocused())
            {
                auto mousePos = ImGui::GetMousePos();
                glm::vec2 mouseContentPos{mousePos.x - windowPos.x - imagePos.x, mousePos.y - windowPos.y - imagePos.y};

                glm::vec2 screenUV = mouseContentPos / glm::vec2{imageWidth, imageHeight};

                auto mainCam = GetCurrentlyActiveCamera();
                if (mainCam != nullptr)
                {
                    Ray ray = mainCam->ScreenUVToWorldSpaceRay(screenUV);

                    std::vector<Intersected> intersected;
                    intersected.reserve(32);
                    std::vector<GameObject*> results;
                    Gizmos::PickGizmos(ray, results);
                    if (results.empty())
                    {
                        PickGameObjectFromScene(ray, screenUV, intersected);
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
                        [](const Intersected& l, const Intersected& r) { return l.distance < r.distance; }
                    );

                    GameObject* selected = nullptr;
                    if (iter != intersected.end())
                    {
                        selected = iter->go;
                    }

                    if (selected)
                    {
                        EditorState::selectedObject = selected->GetSRef();
                    }
                    else
                    {
                        EditorState::selectedObject = nullptr;
                    }
                }
            }
        }

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

                GameObject* go = dynamic_cast<GameObject*>(EditorState::selectedObject.Get());
                if (go)
                {
                    auto model = go->GetModelMatrix();
                    ImGui::SetCursorPos(imagePos);
                    EditTransform(*mainCam, model, proj, rect);
                    if (ImGuizmo::IsUsing())
                        go->SetModelMatrix(model);
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
                mainCam->GetGameObject()->SetModelMatrix(glm::inverse(view));
            }
        }
    }

    ImGui::End();
    return open;
}

void GameView::EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& proj, const glm::vec4& rect)
{
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);

    if (ImGui::IsWindowFocused() && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W))
            mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            mCurrentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) // r Key
            mCurrentGizmoOperation = ImGuizmo::SCALE;
    }

    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
        mCurrentGizmoOperation = ImGuizmo::SCALE;

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
            mCurrentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
            mCurrentGizmoMode = ImGuizmo::WORLD;
    }
    glm::mat4 view = camera.GetViewMatrix();
    ImGuizmo::Manipulate(
        &view[0][0],
        &proj[0][0],
        mCurrentGizmoOperation,
        mCurrentGizmoMode,
        &matrix[0][0],
        NULL,
        nullptr /* useSnap ? &snap.x : NULL */
    );
}

void GameView::ChangeGameScreenResolution(glm::ivec2 resolution)
{
    CreateRenderData(resolution.x, resolution.y);
}

void GameView::FocusOnObject(Camera& cam, GameObject& gameObject)
{
    glm::vec3 center = gameObject.GetPosition();
    cam.GetGameObject()->SetPosition(center);
}

Camera* GameView::GetCurrentlyActiveCamera()
{
    Camera* mainCam = nullptr;
    if (useViewCamera)
        mainCam = editorCamera;
    else
        mainCam = EditorState::activeScene->GetMainCamera();
    return mainCam;
}
} // namespace Editor
