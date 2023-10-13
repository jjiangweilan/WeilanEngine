#include "GameView.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Time.hpp"
#include "EditorState.hpp"
#include "GameEditor.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "ThirdParty/imgui/ImGuizmo.h"
#include "ThirdParty/imgui/imgui.h"
#include "spdlog/spdlog.h"

namespace Engine::Editor
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

        editorCamera->GetGameObject()->GetTransform()->SetPosition({pos[0], pos[1], pos[2]});
        editorCamera->GetGameObject()->GetTransform()->SetRotation(glm::quat{rot[0], rot[1], rot[2], rot[3]});
        editorCamera->GetGameObject()->GetTransform()->SetScale({scale[0], scale[1], scale[2]});
    }

    ImGuizmo::AllowAxisFlip(false);

    CreateRenderData(1960, 1080);
}

static void EditorCameraWalkAround(Camera& editorCamera)
{
    static ImVec2 lastMouseDelta = ImVec2(0, 0);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        auto tsm = editorCamera.GetGameObject()->GetTransform();
        auto pos = tsm->GetPosition();
        glm::mat4 model = tsm->GetModelMatrix();
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
            dir -= forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            dir += forward * speed;
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
        tsm->SetPosition(pos);

        auto mouseLastClickDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0);
        glm::vec2 mouseDelta = {mouseLastClickDelta.x - lastMouseDelta.x, mouseLastClickDelta.y - lastMouseDelta.y};
        lastMouseDelta = mouseLastClickDelta;
        auto upDown = glm::radians(mouseDelta.y * 100) * Time::DeltaTime();
        auto leftRight = glm::radians(mouseDelta.x * 100) * Time::DeltaTime();

        auto eye = tsm->GetPosition();
        auto lookAtDelta = leftRight * right - upDown * up;
        auto final = glm::lookAt(eye, eye - forward + lookAtDelta, glm::vec3(0, 1, 0));
        tsm->SetModelMatrix(glm::inverse(final));
    }
    else
    {
        lastMouseDelta = ImVec2(0, 0);
    }
}

void GameView::CreateRenderData(uint32_t width, uint32_t height)
{
    GetGfxDriver()->WaitForIdle();
    renderer = std::make_unique<SceneRenderer>(*GameEditor::instance->GetEngine()->assetDatabase);
    sceneImage = GetGfxDriver()->CreateImage(
        {
            .width = width,
            .height = height,
            .format = Gfx::ImageFormat::R8G8B8A8_SRGB,
            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
            .mipLevels = 1,
            .isCubemap = false,
        },
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );
    renderer->BuildGraph({
        .finalImage = *sceneImage,
        .layout = Gfx::ImageLayout::Shader_Read_Only,
        .accessFlags = Gfx::AccessMask::Shader_Read,
        .stageFlags = Gfx::PipelineStage::Fragment_Shader,
    });
    renderer->Process();

    editorCamera->SetProjectionMatrix(glm::radians(45.0f), width / (float)height, 0.01f, 1000.f);
}

void GameView::Render(Gfx::CommandBuffer& cmd, Scene* scene)
{
    float width = sceneImage->GetDescription().width;
    float height = sceneImage->GetDescription().height;
    cmd.SetViewport({.x = 0, .y = 0, .width = width, .height = height, .minDepth = 0, .maxDepth = 1});
    Rect2D rect = {{0, 0}, {(uint32_t)width, (uint32_t)height}};
    cmd.SetScissor(0, 1, &rect);
    renderer->Render(cmd, *scene);
}

static bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj, float& distance)
{
    auto mr = obj->GetComponent<MeshRenderer>();
    if (mr)
    {
        auto model = obj->GetTransform()->GetModelMatrix();
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
            if (obj == nullptr)
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

bool GameView::Tick()
{
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
        ImGui::EndMenuBar();
    }

    Scene* scene = EditorState::activeScene;
    if (scene && useViewCamera)
    {
        gameCamera = scene->GetMainCamera();
        scene->SetMainCamera(editorCamera);
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

    if (ImGui::BeginPopup("Change Resolution"))
    {
        ImGui::InputInt2("Resolution", (int*)&d.resolution);
        if (ImGui::Button("Confirm"))
        {
            CreateRenderData(d.resolution.x, d.resolution.y);
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

    if (useViewCamera)
        EditorCameraWalkAround(*editorCamera);

    // create scene color if it's null or if the window size is changed
    const auto contentMax = ImGui::GetWindowContentRegionMax();
    const auto contentMin = ImGui::GetWindowContentRegionMin();
    const float contentWidth = contentMax.x - contentMin.x;
    const float contentHeight = contentMax.y - contentMin.y;

    // imgui image
    if (sceneImage)
    {
        float width = sceneImage->GetDescription().width;
        float height = sceneImage->GetDescription().height;
        float imageWidth = width;
        float imageHeight = height;

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
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && ImGui::IsWindowFocused())
            {
                auto mousePos = ImGui::GetMousePos();
                glm::vec2 mouseContentPos{mousePos.x - windowPos.x - imagePos.x, mousePos.y - windowPos.y - imagePos.y};
                GameObject* selected = PickGameObjectFromScene(mouseContentPos / glm::vec2{imageWidth, imageHeight});

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
                    auto model = go->GetTransform()->GetModelMatrix();
                    ImGui::SetCursorPos(imagePos);
                    EditTransform(
                        *mainCam,
                        model,
                        {imagePos.x + windowPos.x, imagePos.y + windowPos.y, imageWidth, imageHeight}
                    );
                    go->GetTransform()->SetModelMatrix(model);
                }
            }
        }
    }

    ImGui::End();
    return open;
}

void GameView::EditTransform(const Camera& camera, glm::mat4& matrix, const glm::vec4& rect)
{
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);

    if (ImGui::IsWindowFocused() && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Q))
            mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_W))
            mCurrentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) // r Key
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
    // float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    // ImGuizmo::DecomposeMatrixToComponents(matrix.m16, matrixTranslation, matrixRotation, matrixScale);
    // ImGui::InputFloat3("Tr", matrixTranslation, 3);
    // ImGui::InputFloat3("Rt", matrixRotation, 3);
    // ImGui::InputFloat3("Sc", matrixScale, 3);
    // ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.m16);

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
            mCurrentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
            mCurrentGizmoMode = ImGuizmo::WORLD;
    }
    // static bool useSnap(false);
    // if (ImGui::IsKeyPressed(83))
    //    useSnap = !useSnap;
    // ImGui::Checkbox("", &useSnap);
    // ImGui::SameLine();
    // vec_t snap;
    // switch (mCurrentGizmoOperation)
    // {
    // case ImGuizmo::TRANSLATE:
    //    snap = config.mSnapTranslation;
    //    ImGui::InputFloat3("Snap", &snap.x);
    //    break;
    // case ImGuizmo::ROTATE:
    //    snap = config.mSnapRotation;
    //    ImGui::InputFloat("Angle Snap", &snap.x);
    //    break;
    // case ImGuizmo::SCALE:
    //    snap = config.mSnapScale;
    //    ImGui::InputFloat("Scale Snap", &snap.x);
    //    break;
    // }
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjectionMatrix();
    proj[1][1] *= -1;
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

} // namespace Engine::Editor
