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
#include "PickObjectFromGameView.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include "ThirdParty/imgui/imgui.h"

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
            auto& scene = *SceneManager::GetActiveScene();

            AssetDatabase::Singleton()->SaveAsset(scene);
            originalScenePath = AssetDatabase::Singleton()->GetAssetPath(scene.GetUUID());
            AssetDatabase::Singleton()->UnloadAsset(scene);

            GameEditor::instance->GetEngine()->ReloadScripts();
            sceneCopy = AssetDatabase::Singleton()->LoadAsset(originalScenePath);
            sceneCopy->SetName("scene copy");
            sceneCopy->SetFlags(AssetState::DontSave);

            gameView->gameCamera = sceneCopy->GetMainCamera();
            SceneManager::SetActiveScene(sceneCopy);
            EditorState::gameLoop->SetScene(*sceneCopy);
            EngineState::GetSingleton().isPlaying = true;
            EditorState::gameLoop->Play();
            Input::SetGameplayInput(true);
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

            GameEditor::instance->GetEngine()->ReloadScripts();
            auto ori = (Scene*)AssetDatabase::Singleton()->LoadAsset(originalScenePath);
            if (ori)
            {
                SceneManager::SetActiveScene(ori);
                EditorState::gameLoop->SetScene(*ori);
            }
            // destroy sceneCopy
            sceneCopy = nullptr;
            Input::SetGameplayInput(false);
        }
    }
};
GameView::GameView() {}
GameView::~GameView() {}

void GameView::Deinit()
{
    playTheGame->Stop(this);
}

void GameView::Init()
{
    playTheGame = std::make_unique<PlayTheGame>();
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

    outlineRawColorPassShader = ShaderLibrary::GetShader(Shaders::PostProcess_OutlineRawColorPass);
    outlineFullScreenPassShader = ShaderLibrary::GetShader(Shaders::PostProcess_OutlineFullScreenPass);

    ChangeGameScreenResolution({1920, 1080});
}

void GameView::CreateRenderData(uint32_t width, uint32_t height)
{
    pendingDeleteSceneImages.push_back({std::move(sceneImage), 0});

    sceneImage = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R8G8B8A8_SRGB),
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );

    SystemInfo::Singleton().SetScreenSize(width, height);
}

void GameView::Render(
    Gfx::CommandBuffer& cmd, const Gfx::ImageIdentifier* gameImage, const Gfx::ImageIdentifier* gameDepthImage
)
{
    glm::float4 renderPassLabelColor{0.4, 0.5, 0.13, 1.0};
    if (gameImage && gameDepthImage)
    {
        cmd.BeginLabel("Game View Blit", &renderPassLabelColor[0]);
        auto outputImage = GetGfxDriver()->GetImageFromRenderGraph(*gameImage);
        if (outputImage)
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
}

bool GameView::Tick()
{
    for (auto& p : pendingDeleteSceneImages)
    {
        p.frameCount += 1;
    }
    pendingDeleteSceneImages.remove_if([](PendingDelete& p) { return p.frameCount > 5; });

    bool open = true;

    visible = ImGui::Begin("Game", &open, ImGuiWindowFlags_MenuBar);
    isWindowFocused = ImGui::IsWindowFocused();

    // seems like ImGui::IsKeyPressed(ImGuiKey_XXXAlt/XXXShift) most of time can't be registered at the same with with
    // MouseWheel so I track the down and release event individually
    if (ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        isAltDown = true;
    if (ImGui::IsKeyReleased(ImGuiKey_LeftAlt))
        isAltDown = false;

    const char* menuSelected = "";
    if (ImGui::BeginMenuBar())
    {
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

    // alway match window size
    // int width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    // int height = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    // if (sceneImage)
    //{
    //    if (firstFrame || width != sceneImage->GetDescription().width || height !=
    //    sceneImage->GetDescription().height)
    //    {
    //        firstFrame = false;
    //        ChangeGameScreenResolution({width, height});
    //    }
    //}

    if (ImGui::BeginPopup("Change Resolution"))
    {
        int2 predefinedSolutions[] = {{1920, 1080}, {2560, 1440}, {-1, -1}};
        const char* predefinedSolutionsText[] = {"1920x1080", "2560x1440", "Custom"};
        const int totalSelectionCount = sizeof(predefinedSolutionsText) / sizeof(const char*);
        static int resolutionSelectionIdx = totalSelectionCount - 1;

        if (ImGui::BeginTable("Resolution Table", 2))
        {
            ImGui::TableNextColumn();
            ImGui::Text("Resolutions");
            ImGui::TableNextColumn();
            if (resolutionSelectionIdx != totalSelectionCount - 1)
            {
                for (; resolutionSelectionIdx < sizeof(predefinedSolutions) / sizeof(int2); resolutionSelectionIdx++)
                {
                    if (d.resolution == predefinedSolutions[resolutionSelectionIdx])
                    {
                        break;
                    }
                }
                // fallback to custom
                if (resolutionSelectionIdx == totalSelectionCount)
                    resolutionSelectionIdx = totalSelectionCount - 1;
            }

            // resolution selection
            {
                ImGui::SetNextItemWidth(100);
                if (ImGui::Combo(
                        "##ResolutionCombo",
                        &resolutionSelectionIdx,
                        predefinedSolutionsText,
                        totalSelectionCount
                    ))
                {
                    if (resolutionSelectionIdx + 1 != totalSelectionCount)
                    {
                        d.resolution = predefinedSolutions[resolutionSelectionIdx];
                        ChangeGameScreenResolution(d.resolution);
                    }
                }
            }

            ImGui::EndTable();
        }

        if (resolutionSelectionIdx + 1 == totalSelectionCount)
        {
            if (ImGui::BeginTable("Custom Resolution", 2))
            {
                ImGui::TableNextColumn();
                ImGui::Text("Custom Resolution");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt2("##Resolution", (int*)&d.resolution);
                ImGui::EndTable();
            }
            if (ImGui::Button("Confirm"))
            {
                ChangeGameScreenResolution(d.resolution);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

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

        auto contentRegionWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        auto contentRegionHeight = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
        auto cursorPos = ImGui::GetCursorPos();
        cursorPos.x = cursorPos.x + contentRegionWidth / 2.0f - imageWidth / 2.0f;
        cursorPos.y = cursorPos.y + contentRegionHeight / 2.0f - imageHeight / 2.0f;
        ImGui::SetCursorPos(cursorPos);

        ImGui::Image(&sceneImage->GetDefaultImageView(), {imageWidth, imageHeight});

        auto logs = HudDebug::Singleton().FlushLogs();
        for (auto log : logs)
        {
            ImGui::Text("%s", log.data());
        }
    }

    ImGui::End();
    return open;
}

void GameView::ChangeGameScreenResolution(glm::ivec2 resolution)
{
    if (resolution.x > 0 && resolution.y > 0)
        CreateRenderData(resolution.x, resolution.y);
}
} // namespace Editor
