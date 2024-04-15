#include "GameView.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Time.hpp"
#include "EditorState.hpp"
#include "GameEditor.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"
#include "spdlog/spdlog.h"

namespace Editor
{
GameView::GameView() {}

void GameView::Init()
{
    editorCameraGO = std::make_unique<GameObject>();
    editorCameraGO->SetName("editor camera");
    editorCamera = editorCameraGO->AddComponent<Camera>();

    if (GameEditor::instance->editorConfig.contains("editorCamera"))
    {
        auto& camJson = GameEditor::instance->editorConfig["editorCamera"];
        std::array<float, 3> pos{0, 0, 0};
        pos = camJson.value("position", pos);
        std::array<float, 4> rot{1, 0, 0, 0};
        rot = camJson.value("rotation", rot);
        std::array<float, 3> scale{1, 1, 1};
        scale = camJson.value("scale", scale);

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

static GameObject* PickGameObjectFromScene(glm::vec2 screenUV)
{
    if (Scene* scene = EditorState::activeScene)
    {
        auto objs = scene->GetAllGameObjects();
        auto mainCam = scene->GetMainCamera();
        Ray ray = mainCam->ScreenUVToWorldSpaceRay(screenUV);

        auto ori = ray.origin;
        auto dir = ray.direction;
        struct Intersected
        {
            GameObject* go;
            float distance;
        };
        std::vector<Intersected> intersected;
        intersected.reserve(32);
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

        auto iter = std::min_element(
            intersected.begin(),
            intersected.end(),
            [](const Intersected& l, const Intersected& r) { return l.distance < r.distance; }
        );

        if (iter != intersected.end())
        {
            return iter->go;
        }
    }

    return nullptr;
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

void GameView::Render(Gfx::CommandBuffer& cmd, const Gfx::RG::ImageIdentifier* gameImage)
{
    if (gameImage)
    {
        // selection outline
        if (GameObject* go = dynamic_cast<GameObject*>(EditorState::selectedObject))
        {
            auto meshRenderer = go->GetComponent<MeshRenderer>();
            if (meshRenderer)
            {
                FrameGraph::DrawList drawList;
                drawList.Add(*meshRenderer);

                Gfx::RG::ImageDescription desc{
                    sceneImage->GetDescription().width,
                    sceneImage->GetDescription().height,
                    sceneImage->GetDescription().format
                };
                cmd.AllocateAttachment(outlineSrcRT, desc);

                Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
                outlineSrcPass.SetAttachment(0, outlineSrcRT);
                Rect2D rect{{0, 0}, {sceneImage->GetDescription().width, sceneImage->GetDescription().height}};
                cmd.SetScissor(0, 1, &rect);
                cmd.SetViewport(Gfx::Viewport{
                    0,
                    0,
                    static_cast<float>(rect.extent.width),
                    static_cast<float>(rect.extent.height),
                    0,
                    1
                });
                cmd.BeginRenderPass(outlineSrcPass, clears);
                for (auto& draw : drawList)
                {
                    cmd.BindShaderProgram(
                        outlineRawColorPassShader->GetDefaultShaderProgram(),
                        outlineRawColorPassShader->GetDefaultShaderConfig()
                    );
                    cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                    cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                    cmd.SetPushConstant(draw.shader->GetShaderProgram(0, 0), (void*)&draw.pushConstant);
                    cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                }
                cmd.EndRenderPass();

                outlieFinalPass.SetAttachment(0, *gameImage);
                cmd.SetTexture("mainTex", outlineSrcRT);
                cmd.BeginRenderPass(outlieFinalPass, clears);
                cmd.BindShaderProgram(
                    outlineFullScreenPassShader->GetDefaultShaderProgram(),
                    outlineFullScreenPassShader->GetDefaultShaderConfig()
                );
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();

                // cmd.SetTexture("mainTex", *graphOutputImage);
            }
        }

        if (gameImage)
        {
            auto outputImage = GetGfxDriver()->GetImageFromRenderGraph(*gameImage);
            if (outputImage)
                cmd.Blit(outputImage, sceneImage.get());
        }
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
    if (ImGui::BeginMenuBar())
    {
        const char* toggleViewCamera = "Toggle View Camera: On";
        if (!useViewCamera)
            toggleViewCamera = "Toggle View Camera: Off";
        if (ImGui::MenuItem(toggleViewCamera))
        {
            useViewCamera = !useViewCamera;
        }
        if (ImGui::MenuItem("Resolution"))
        {
            menuSelected = "Change Resolution";
        }
        if (ImGui::MenuItem("Auto Resize"))
        {
            menuSelected = "Auto Resize";
        }
        if (ImGui::MenuItem("Play"))
        {
            menuSelected = "Play";
        }
        if (ImGui::MenuItem("Pause"))
        {
            menuSelected = "Pause";
        }
        ImGui::EndMenuBar();
    }

    Scene* scene = EditorState::activeScene;
    // TODO(bug): can't switch back to game camera
    if (scene && useViewCamera)
    {
        auto gameCamera = scene->GetMainCamera();
        if (gameCamera)
        {
            if (gameCamera != editorCamera)
            {
                this->gameCamera = gameCamera;
                editorCamera->GetGameObject()->SetScene(gameCamera->GetGameObject()->GetScene());
                editorCamera->SetFrameGraph(gameCamera->GetFrameGraph());
                auto framegraph = editorCamera->GetFrameGraph();
                if (framegraph && !framegraph->IsCompiled())
                    editorCamera->GetFrameGraph()->Compile();
            }
            scene->SetMainCamera(editorCamera);
        }
    }
    else if (gameCamera)
    {
        scene->SetMainCamera(gameCamera);
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
        if (EditorState::activeScene)
            EditorState::gameLoop->Play();
    }
    else if (strcmp(menuSelected, "Pause") == 0)
    {
        EditorState::gameLoop->Stop();
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
            if (GameObject* selected = dynamic_cast<GameObject*>(EditorState::selectedObject))
                EditorState::selectedObject = scene->CopyGameObject(*selected);
        }
    }

    // auto resize first frame
    if (firstFrame)
    {
        int width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        int height = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
        if (width == 0 || height == 0)
        {
            width = 1920;
            height = 1080;
        }
        ChangeGameScreenResolution({width, height});
    }
    firstFrame = false;

    if (useViewCamera)
        EditorCameraWalkAround(*editorCamera);

    // create scene color if it's null or if the window size is changed
    const auto contentMax = ImGui::GetWindowContentRegionMax();
    const auto contentMin = ImGui::GetWindowContentRegionMin();
    const float contentWidth = contentMax.x - contentMin.x;
    const float contentHeight = contentMax.y - contentMin.y;

    // imgui image
    if (scene)
    {
        auto mainCamera = scene->GetMainCamera();
        if (mainCamera)
        {

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

                auto imagePos = ImGui::GetCursorPos();
                ImGui::Image(&sceneImage->GetDefaultImageView(), {imageWidth, imageHeight});

                auto windowPos = ImGui::GetWindowPos();
                if (!ImGuizmo::IsUsing())
                {
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() &&
                        ImGui::IsWindowFocused())
                    {
                        auto mousePos = ImGui::GetMousePos();
                        glm::vec2 mouseContentPos{
                            mousePos.x - windowPos.x - imagePos.x,
                            mousePos.y - windowPos.y - imagePos.y
                        };
                        GameObject* selected =
                            PickGameObjectFromScene(mouseContentPos / glm::vec2{imageWidth, imageHeight});

                        if (selected)
                        {
                            EditorState::selectedObject = selected;
                        }
                        else
                        {
                            EditorState::selectedObject = nullptr;
                        }
                    }
                }

                Scene* scene = EditorState::activeScene;
                if (scene != nullptr)
                {
                    auto mainCam = scene->GetMainCamera();
                    if (mainCam)
                    {
                        if (GameObject* go = dynamic_cast<GameObject*>(EditorState::selectedObject))
                        {
                            auto model = go->GetModelMatrix();
                            ImGui::SetCursorPos(imagePos);
                            EditTransform(
                                *mainCam,
                                model,
                                {imagePos.x + windowPos.x, imagePos.y + windowPos.y, imageWidth, imageHeight}
                            );
                            go->SetModelMatrix(model);
                        }
                    }
                }
            }
        }
    }

    ImGui::End();
    return open;
}

void GameView::EditTransform(Camera& camera, glm::mat4& matrix, const glm::vec4& rect)
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
    glm::mat4 proj = camera.GetProjectionMatrix();
    proj[1] *= -1;
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);
    ImGuizmo::SetRect(rect.x, rect.y, rect.z, rect.w);
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

} // namespace Editor
