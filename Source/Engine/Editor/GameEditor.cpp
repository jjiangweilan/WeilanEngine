#include "GameEditor.hpp"
#include "Core/Time.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include "spdlog/spdlog.h"

namespace Engine::Editor
{

GameEditor::GameEditor(Scene& scene) : scene(scene)
{
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(GetGfxDriver()->GetSDLWindow());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    editorCameraGO = std::make_unique<GameObject>();
    editorCameraGO->SetName("editor camera");
    editorCamera = editorCameraGO->AddComponent<Camera>();
};

GameEditor::~GameEditor()
{
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void SceneTree(Transform* transform)
{
    if (ImGui::TreeNode(transform->GetGameObject()->GetName().c_str()))
    {
        GameObject* go = transform->GetGameObject();
        for (auto& c : go->GetComponents())
        {
            if (c->GetName() == "Transform")
            {
                auto pos = transform->GetPosition();
                if (ImGui::InputFloat3("Position", &pos[0]))
                {
                    transform->SetPosition(pos);
                }

                auto rotation = transform->GetRotation();
                if (ImGui::InputFloat3("rotation", &rotation[0]))
                {
                    transform->SetRotation(rotation);
                }
            }
            if (c->GetName() == "Light")
            {
                Light* light = static_cast<Light*>(c.Get());
                float intensity = light->GetIntensity();
                ImGui::DragFloat("intensity", &intensity);
                light->SetIntensity(intensity);
            }
        }

        for (auto child : transform->GetChildren())
        {
            SceneTree(child.Get());
        }
        ImGui::TreePop();
    }
}

static void SceneTree(Scene& scene)
{
    ImGui::Begin("Scene");
    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root->GetTransform().Get());
    }
    ImGui::End();
}

static void MainMenuBar(Scene& scene, Camera*& gameCamera, Camera*& editorCamera)
{
    ImGui::BeginMainMenuBar();
    if (ImGui::MenuItem("Editor Camera"))
    {
        gameCamera = scene.GetMainCamera();
        scene.SetMainCamera(editorCamera);
    }
    if (ImGui::MenuItem("Game Camera"))
    {
        scene.SetMainCamera(gameCamera);
        gameCamera = nullptr;
    }
    ImGui::EndMainMenuBar();
}

static void EditorCameraWalkAround(Camera& editorCamera)
{
    auto tsm = editorCamera.GetGameObject()->GetTransform();
    auto pos = tsm->GetPosition();
    glm::mat4 model = tsm->GetModelMatrix();
    glm::vec3 right = glm::normalize(model[0]);
    glm::vec3 up = glm::normalize(model[1]);
    glm::vec3 forward = glm::normalize(model[2]);

    ImGui::Begin("Test");
    static float testSpeed = 100;
    ImGui::DragFloat("Speed", &testSpeed);
    auto camPos = tsm->GetPosition();
    if (ImGui::InputFloat3("Editor Cam Pos", &camPos[0]))
    {
        tsm->SetPosition(camPos);
    }

    if (ImGui::Button("Reset"))
    {
        editorCamera.GetGameObject()->GetTransform()->SetRotation({0, 0, 0});
    }
    ImGui::End();

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered())
    {
        return;
    }

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

    static ImVec2 lastMouseDelta = ImVec2(0, 0);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        auto rotation = tsm->GetRotationQuat();
        auto mouseLastClickDelta = ImGui::GetMouseDragDelta(0, 0);
        glm::vec2 mouseDelta = {mouseLastClickDelta.x - lastMouseDelta.x, mouseLastClickDelta.y - lastMouseDelta.y};
        lastMouseDelta = mouseLastClickDelta;
        auto upDown = glm::radians(mouseDelta.y * testSpeed) * Time::DeltaTime();
        auto leftRight = glm::radians(mouseDelta.x * testSpeed) * Time::DeltaTime();

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

void GameEditor::OnWindowResize(int32_t width, int32_t height)
{
    editorCamera->SetProjectionMatrix(glm::radians(45.0f), width / (float)height, 0.01f, 1000.f);
}

void GameEditor::Tick()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    MainMenuBar(scene, gameCamera, editorCamera);
    bool isEditorCameraActive = gameCamera != nullptr;
    if (isEditorCameraActive)
        EditorCameraWalkAround(*editorCamera);
    SceneTree(scene);

    ImGui::EndFrame();
}
} // namespace Engine::Editor
