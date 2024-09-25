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
            EditorState::gameLoop->SetScene(*sceneCopy, *gameView->GetCurrentlyActiveCamera());
            gameView->editorCameraGO->SetScene(sceneCopy.get());

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
    if (EditorState::activeScene)
    {
        gameCamera = EditorState::activeScene->GetMainCamera();
        editorCamera->GetGameObject()->SetScene(EditorState::activeScene);
        if (gameCamera)
        {
            auto fg = gameCamera->GetFrameGraph();
            if (fg)
            {
                editorCamera->SetFrameGraph(fg);
                if (!fg->IsCompiled())
                    fg->Compile();

                if (fg->IsCompiled())
                {
                    spdlog::info("FrameGraph initial compiling failed");
                }
            }
        }
        EditorState::gameLoop->SetScene(*EditorState::activeScene, *editorCamera);
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

    outlineRawColorPassShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/OutlineRawColorPass.shad")
        );
    outlineFullScreenPassShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/OutlineFullScreenPass.shad"
        ));

    editorWorldSpaceGrid.plane =
        static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.glb"))
            ->GetMeshes()[0]
            .get();
    editorWorldSpaceGrid.gridShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/PlaneGrid.shad"));

    ChangeGameScreenResolution({256, 256});
}

static bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj, float& distance)
{
    distance = std::numeric_limits<float>::max();
    auto mr = obj->GetComponent<MeshRenderer>();
    if (mr)
    {
        auto model = obj->GetWorldMatrix();
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
                    float newDistance = -1;
                    if (glm::intersectRayTriangle(ori, dir, v0, v1, v2, bary, newDistance))
                    {
                        distance = glm::min(distance, newDistance);
                    }
                }
            }
        }
    }

    return distance > 0;
}

struct PickGameObjectFromScene
{
    // main thread populates pending vectors, while worker threads process the pending. workers takes the target
    // GameObject using the `consumerIndex`, each process of a GameObject appends an `Intersected` in `results` if it's
    // intersected with the GameObject. `consumerIndex` is incrementally increased by 1 each time the worker takes a
    // GameObject to process
private:
    std::vector<GameObject*> pending;
    std::atomic<int> consumerIndex{0};

public:
    std::vector<Intersected> results;

    void operator()(Scene& scene, const Ray& ray, glm::vec2 screenUV, std::vector<Intersected>& intersected)
    {
        pending = scene.GetAllGameObjects();
        std::vector<std::thread> threads;
        std::mutex m;
        int maxThreads = std::max(1.0f, std::thread::hardware_concurrency() - 2.0f);
        for (int i = 0; i < maxThreads; ++i)
        {
            threads.push_back(std::thread(
                [this, &m, &intersected, &ray]()
                {
                    while (consumerIndex < pending.size())
                    {
                        int index = consumerIndex.fetch_add(1);

                        GameObject* obj = pending[index];
                        if (obj == nullptr || !obj->IsEnabled())
                            continue;

                        auto ori = ray.origin;
                        auto dir = ray.direction;
                        float distance = std::numeric_limits<float>::max();
                        if (IsRayObjectIntersect(ori, dir, obj, distance))
                        {
                            std::scoped_lock lock(m);
                            intersected.push_back(Intersected{obj, distance});
                        }
                    };
                }
            ));
        }

        for (auto& t : threads)
            t.join();
    }
};

// static void PickGameObjectFromScene(const Ray& ray, glm::vec2 screenUV, std::vector<Intersected>& intersected)
// {
//     if (Scene* scene = EditorState::activeScene)
//     {
//         auto objs = scene->GetAllGameObjects();
//
//         auto ori = ray.origin;
//         auto dir = ray.direction;
//         for (auto obj : objs)
//         {
//             if (obj == nullptr || !obj->IsEnabled())
//                 continue;
//
//             float distance = std::numeric_limits<float>::max();
//             if (IsRayObjectIntersect(ori, dir, obj, distance))
//             {
//                 intersected.push_back(Intersected{obj, distance});
//             }
//         }
//     }
// }

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
        glm::vec3 forward = glm::normalize(model[2]);

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
        auto final = glm::lookAt(eye, eye - (forward + lookAtDelta), glm::vec3(0, 1, 0));
        final = glm::inverse(final);
        go->SetWorldMatrix(final);
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
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {1.0f, 0}};
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
            cmd.BeginRenderPass(outlineSrcPass, clears);
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
                }
            }
            cmd.EndRenderPass();
        }

        // draw outline and gizmos
        gameImagePass.SetAttachment(0, *gameImage);
        if (gameDepthImage)
            gameImagePass.SetAttachment(1, *gameDepthImage);
        cmd.BeginRenderPass(gameImagePass, clears);

        if (hasGameObjectSelected)
        {
            cmd.SetTexture("mainTex", outlineSrcRT);
            cmd.BindShaderProgram(
                outlineFullScreenPassShader->GetDefaultShaderProgram(),
                outlineFullScreenPassShader->GetDefaultShaderConfig()
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

    // seems like ImGui::IsKeyPressed(ImGuiKey_XXXAlt/XXXShift) most of time can't be registered at the same with with
    // MouseWheel so I track the down and release event individually
    if (ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        isAltDown = true;
    if (ImGui::IsKeyReleased(ImGuiKey_LeftAlt))
        isAltDown = false;

    // sync camera settigns
    if (gameCamera && editorCamera)
    {
        editorCamera->SetSkybox(gameCamera->GetSkybox().Get());
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

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C))
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
                        if (EditorState::activeScene)
                        {
                            PickGameObjectFromScene()(*EditorState::activeScene, ray, screenUV, intersected);
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
                    auto model = go->GetWorldMatrix();

                    glm::vec3 avgPos = glm::vec3(0);

                    for (auto& s : selectedObjects)
                    {
                        auto go = static_cast<GameObject*>(s.Get());
                        avgPos += go->GetPosition();
                    }
                    avgPos /= selectedObjects.size();
                    model[3] = glm::vec4(avgPos, 1.0f);

                    ImGui::SetCursorPos(imagePos);

                    glm::mat4 deltaMatrix;
                    EditTransform(*mainCam, model, deltaMatrix, proj);

                    // directly multiplying deltaMatrix resulting in exponentially scaling the object
                    glm::vec3 scale = glm::vec3(
                        length(glm::vec3(deltaMatrix[0])),
                        length(glm::vec3(deltaMatrix[1])),
                        length(glm::vec3(deltaMatrix[2]))
                    );
                    deltaMatrix[0] /= scale.x;
                    deltaMatrix[1] /= scale.y;
                    deltaMatrix[2] /= scale.z;

                    if (ImGuizmo::IsUsing())
                    {

                        for (auto& s : selectedObjects)
                        {
                            auto go = static_cast<GameObject*>(s.Get());
                            glm::mat4 model = go->GetWorldMatrix();

                            // handle scale
                            glm::mat4 scaleMatrix = glm::mat4(
                                glm::vec4(scale.x, 0.0, 0.0f, 0.0f),
                                glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
                                glm::vec4(0.0f, 0.0f, scale.z, 0.0f),
                                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
                            );

                            // get rotation
                            glm::mat4 rot = glm::mat3(model);
                            rot[0] = glm::vec4(glm::normalize(glm::vec3(rot[0])), 1.0f);
                            rot[1] = glm::vec4(glm::normalize(glm::vec3(rot[1])), 1.0f);
                            rot[2] = glm::vec4(glm::normalize(glm::vec3(rot[2])), 1.0f);
                            glm::mat4 invRot = glm::inverse(rot);

                            model[3] -= glm::vec4(avgPos, 0.0f);
                            model[3][3] = 0.0f;
                            model = rot * scaleMatrix * invRot * model;
                            model[3] += glm::vec4(avgPos, 0.0f);
                            model[3][3] = 1.0f;

                            // handle translation and rotation
                            model = deltaMatrix * model;

                            go->SetWorldMatrix(model);
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
                mainCam->GetGameObject()->SetWorldMatrix(glm::inverse(view));
            }
        }
    }

    ImGui::End();
    return open;
}

void GameView::EditTransform(Camera& camera, glm::mat4& matrix, glm::mat4& deltaMatrix, glm::mat4& proj)
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

    ImGui::Checkbox("Snap to xy", &gameObjectConfigs.useSnap);
    glm::mat4 view = camera.GetViewMatrix();
    ImGuizmo::Manipulate(
        &view[0][0],
        &proj[0][0],
        mCurrentGizmoOperation,
        mCurrentGizmoMode,
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
