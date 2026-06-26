#include "Editor/GameView.hpp"

#include "Editor/EditorState.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/Gizmos/Gizmo.hpp"
#include "Editor/HudDebug.hpp"
#include "Editor/PickObjectFromGameView.hpp"
#include "Engine/Core/EngineState.hpp"
#include "Engine/Core/GameLoop.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/Physics/JoltDebugRenderer.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/MiddleLayer/SystemInfo.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/WeilanEngine.hpp"

namespace Editor
{
namespace
{
const int2 gameViewPredefinedResolutions[] = {{1920, 1080}, {2560, 1440}, {-1, -1}};
const char* gameViewPredefinedResolutionText[] = {"1920x1080", "2560x1440", "Custom"};
constexpr int gameViewResolutionSelectionCount = sizeof(gameViewPredefinedResolutionText) / sizeof(const char*);
constexpr int gameViewCustomResolutionSelectionIdx = gameViewResolutionSelectionCount - 1;

bool IsValidGameViewResolution(glm::ivec2 resolution)
{
    return resolution.x > 0 && resolution.y > 0;
}

int GetGameViewResolutionSelectionIndex(glm::ivec2 resolution)
{
    for (int i = 0; i < gameViewCustomResolutionSelectionIdx; i++)
    {
        if (resolution == gameViewPredefinedResolutions[i])
            return i;
    }
    return gameViewCustomResolutionSelectionIdx;
}
} // namespace

struct GameView::PlayTheGame
{
    PlayTheGame() {}
    ObjPtr<Scene> sceneCopy;
    AssetPath originalScenePath;
    bool played = false;

    void Play(GameView* gameView)
    {
        if (!played)
        {
            UI::Instance().Destroy(); // this destroys the editor UI context

            played = true;
            auto& scene = *SceneManager::GetActiveScene();

            AssetDatabase::Singleton()->SaveAsset(scene);
            UUID sceneUUID = scene.GetUUID();
            originalScenePath = AssetDatabase::Singleton()->GetAssetPath(scene.GetUUID());
            AssetDatabase::Singleton()->UnloadAsset(scene);

            GameEditor::instance->GetEngine()->ReloadScripts();
            UI::Instance().Init(); // this creates the game UI context

            sceneCopy = AssetDatabase::Singleton()->LoadScene(sceneUUID);
            sceneCopy->SetName("scene copy");
            sceneCopy->SetFlags(AssetState::DontSave);

            gameView->gameCamera = sceneCopy->GetMainCamera();
            SceneManager::SetActiveScene(sceneCopy);
            EditorState::GetGameLoop()->SetScene(*sceneCopy);
            EngineState::GetSingleton().isPlaying = true;
            EditorState::GetGameLoop()->Play();
            Input::SetGameplayInput(true);
        }
    }

    void Stop(GameView* gameView)
    {
        if (played)
        {
            played = false;
            // stop execution of the loop
            EditorState::GetGameLoop()->Stop();

            // resume editor state
            EngineState::GetSingleton().isPlaying = false;
            AssetDatabase::Singleton()->UnloadAsset(*sceneCopy);

            UI::Instance().Destroy(); // destroys the game UI context
            UI::Instance().Init(); // this creates the editor UI context
            GameEditor::instance->GetEngine()->ReloadScripts();
            auto ori = (Scene*)AssetDatabase::Singleton()->LoadAsset(originalScenePath);
            if (ori)
            {
                SceneManager::SetActiveScene(ori);
                EditorState::GetGameLoop()->SetScene(*ori);
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
    UI::Instance().Destroy();
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

    glm::ivec2 resolution = gameViewPredefinedResolutions[0];
    resolutionSelectionIdx = 0;

    auto& editorState = GameEditor::instance->editorState;
    if (editorState.contains("gameView") && editorState["gameView"].is_object())
    {
        auto& gameViewState = editorState["gameView"];
        glm::ivec2 savedResolution = resolution;
        bool hasSavedResolution = false;
        if (gameViewState.contains("resolution") && gameViewState["resolution"].is_array() &&
            gameViewState["resolution"].size() == 2 && gameViewState["resolution"][0].is_number_integer() &&
            gameViewState["resolution"][1].is_number_integer())
        {
            savedResolution = {
                gameViewState["resolution"][0].get<int>(),
                gameViewState["resolution"][1].get<int>()
            };
            hasSavedResolution = IsValidGameViewResolution(savedResolution);
        }

        int savedSelectionIdx = resolutionSelectionIdx;
        if (gameViewState.contains("resolutionSelectionIndex") &&
            gameViewState["resolutionSelectionIndex"].is_number_integer())
        {
            savedSelectionIdx = gameViewState["resolutionSelectionIndex"].get<int>();
        }
        if (savedSelectionIdx >= 0 && savedSelectionIdx < gameViewResolutionSelectionCount)
        {
            resolutionSelectionIdx = savedSelectionIdx;
            if (resolutionSelectionIdx == gameViewCustomResolutionSelectionIdx)
            {
                if (hasSavedResolution)
                    resolution = savedResolution;
            }
            else
            {
                resolution = gameViewPredefinedResolutions[resolutionSelectionIdx];
            }
        }
        else if (hasSavedResolution)
        {
            resolution = savedResolution;
            resolutionSelectionIdx = GetGameViewResolutionSelectionIndex(resolution);
        }
    }

    ChangeGameScreenResolution(resolution);
    UI::Instance().Init();
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
    pendingDeleteSceneImages.remove_if([](PendingDelete& p)
                                       { return p.frameCount > 5; });

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
        bool rmlDebuggerVisible = UI::Instance().IsRmlDebuggerVisible();
        if (ImGui::MenuItem("Rml Debugger", nullptr, rmlDebuggerVisible))
        {
            menuSelected = "Rml Debugger";
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
        if (resolutionSelectionIdx != gameViewCustomResolutionSelectionIdx)
            resolutionSelectionIdx = GetGameViewResolutionSelectionIndex(d.resolution);
    }
    else if (strcmp(menuSelected, "Auto Resize") == 0)
    {
        int width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        int height = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
        ChangeGameScreenResolution({width, height});
        resolutionSelectionIdx = GetGameViewResolutionSelectionIndex({width, height});
    }
    else if (strcmp(menuSelected, "Rml Debugger") == 0)
    {
        UI::Instance().ToggleRmlDebugger();
    }
    else if (strcmp(menuSelected, "Play") == 0)
    {
        playTheGame->Play(this);
    }
    else if (strcmp(menuSelected, "Pause") == 0)
    {
        EditorState::GetGameLoop()->Stop();
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
        if (ImGui::BeginTable("Resolution Table", 2))
        {
            ImGui::TableNextColumn();
            ImGui::Text("Resolutions");
            ImGui::TableNextColumn();

            // resolution selection
            {
                ImGui::SetNextItemWidth(100);
                if (ImGui::Combo(
                        "##ResolutionCombo",
                        &resolutionSelectionIdx,
                        gameViewPredefinedResolutionText,
                        gameViewResolutionSelectionCount
                    ))
                {
                    if (resolutionSelectionIdx != gameViewCustomResolutionSelectionIdx)
                    {
                        d.resolution = gameViewPredefinedResolutions[resolutionSelectionIdx];
                        ChangeGameScreenResolution(d.resolution);
                    }
                }
            }

            ImGui::EndTable();
        }

        if (resolutionSelectionIdx == gameViewCustomResolutionSelectionIdx)
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

        auto windowPos = ImGui::GetWindowPos();
        auto imagePos = ImGui::GetCursorPos();
        SystemInfo::Singleton().SetGameViewOrigin(int2(windowPos.x + imagePos.x, windowPos.y + imagePos.y));

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
    if (IsValidGameViewResolution(resolution))
    {
        d.resolution = resolution;
        CreateRenderData(resolution.x, resolution.y);
    }
}

glm::ivec2 GameView::GetGameScreenResolution() const
{
    if (sceneImage)
    {
        return {sceneImage->GetDescription().width, sceneImage->GetDescription().height};
    }
    if (IsValidGameViewResolution(d.resolution))
    {
        return d.resolution;
    }
    return gameViewPredefinedResolutions[0];
}
} // namespace Editor
