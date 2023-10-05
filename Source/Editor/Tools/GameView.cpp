#include "GameView.hpp"
#include "Core/Time.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{
GameView::GameView() {}

void GameView::Init()
{
    editorCameraGO = std::make_unique<GameObject>();
    editorCameraGO->SetName("editor camera");
    editorCamera = editorCameraGO->AddComponent<Camera>();

    CreateRenderData(1080, 960);
}

static void EditorCameraWalkAround(Camera& editorCamera)
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

    static ImVec2 lastMouseDelta = ImVec2(0, 0);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        auto rotation = tsm->GetRotationQuat();
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
    renderer = std::make_unique<SceneRenderer>();
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

bool GameView::Tick()
{
    bool open = true;

    ImGui::Begin("Game View", &open, ImGuiWindowFlags_MenuBar);

    const char* menuSelected = "";
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::MenuItem("Resolution"))
        {
            menuSelected = "Change Resolution";
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

    if (ImGui::IsWindowFocused())
    {
        bool isEditorCameraActive = gameCamera != nullptr;
        if (isEditorCameraActive)
            EditorCameraWalkAround(*editorCamera);
    }

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

        ImGui::Image(&sceneImage->GetDefaultImageView(), {imageWidth, imageHeight});
    }

    ImGui::End();
    return open;
}
} // namespace Engine::Editor
