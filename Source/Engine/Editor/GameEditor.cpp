#include "GameEditor.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_sdl.h"

#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "EditorRegister.hpp"
#include "Extension/Inspector/BuildInInspectors.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderLoader.hpp"

namespace Engine::Editor
{
GameEditor::GameEditor(RefPtr<Gfx::GfxDriver> gfxDriver, RefPtr<ProjectManagement> projectManagement)
    : projectManagement(projectManagement), gfxDriver(gfxDriver)
{}

GameEditor::~GameEditor()
{
    projectManagement->Save();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void GameEditor::Init()
{
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(gfxDriver->GetSDLWindow());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::GetIO().ConfigFlags += ImGuiConfigFlags_DockingEnable;
    ImGui::LoadIniSettingsFromDisk("imgui.ini");

    gameEditorRenderer = MakeUnique<GameEditorRenderer>();
    editorContext = MakeUnique<EditorContext>();
    sceneTreeWindow = MakeUnique<SceneTreeWindow>(editorContext);
    inspector = MakeUnique<InspectorWindow>(editorContext);
    assetExplorer = MakeUnique<AssetExplorer>(editorContext);
    gameSceneWindow = MakeUnique<GameSceneWindow>(editorContext);
    projectManagementWindow = MakeUnique<ProjectManagementWindow>(editorContext, projectManagement);
    projectWindow = nullptr;

    InitializeBuiltInInspector();

    // auto p = AssetDatabase::Instance()->GetShader("Internal/SimpleBlend")->GetShaderProgram();
    // res = Gfx::GfxDriver::Instance()->CreateShaderResource(p, Gfx::ShaderResourceFrequency::Shader);
    // res->SetTexture("mainTex", imGuiData.editorRT);

    renderPass = Gfx::GfxDriver::Instance()->CreateRenderPass();
    Gfx::RenderPass::Attachment c;
    c.image = gfxDriver->GetSwapChainImageProxy();
    renderPass->AddSubpass({c}, std::nullopt);
}

void GameEditor::ProcessEvent(const SDL_Event& event) { ImGui_ImplSDL2_ProcessEvent(&event); }

bool GameEditor::IsProjectInitialized() { return projectManagement->IsInitialized(); }

void GameEditor::Tick()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    // ImGui::ShowDemoWindow(nullptr);
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    DrawMainMenu();

    if (projectManagement->IsInitialized())
    {
        sceneTreeWindow->Tick();
        inspector->Tick();
        assetExplorer->Tick();
        gameSceneWindow->Tick(gameColorImage, gameDepthImage);
    }
    else
    {
        projectManagementWindow->Tick();
    }
    if (projectWindow != nullptr)
    {
        bool open = true;
        projectWindow->Tick(open);
        if (!open) projectWindow = nullptr;
    }
}

void GameEditor::Render(RefPtr<CommandBuffer> cmdBuf, RGraph::ResourceStateTrack& stateTrack)
{
    ImGui::Render();
    return;
    gameEditorRenderer->Render(cmdBuf.Get(), stateTrack);

    // gameSceneWindow->RenderSceneGUI(cmdBuf);
    // RenderEditor(cmdBuf);

    // static std::vector<Gfx::ClearValue> clears = {{{{0, 0, 0, 0}}}};
    // Rect2D scissor;
    // scissor.offset = {0, 0};
    // scissor.extent = {static_cast<uint32_t>(ImGui::GetIO().DisplaySize.x),
    //                   static_cast<uint32_t>(ImGui::GetIO().DisplaySize.y)};
    // cmdBuf->SetScissor(0, 1, &scissor);
    // cmdBuf->BeginRenderPass(renderPass, clears);
    // cmdBuf->BindShaderProgram(res->GetShader(), res->GetShader()->GetDefaultShaderConfig());
    // cmdBuf->BindResource(res);
    // cmdBuf->Draw(6, 1, 0, 0);
    // cmdBuf->EndRenderPass();
    // cmdBuf->Blit(imGuiData.editorRT, gfxDriver->GetSwapChainImageProxy());
}

void GameEditor::RenderEditor(RefPtr<CommandBuffer> cmdBuf) {}

void GameEditor::DrawMainMenu()
{
    ImGui::BeginMainMenuBar();

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Project"))
        {
            projectWindow = MakeUnique<ProjectWindow>(editorContext, projectManagement);
        }
        ImGui::EndMenu();
    }

    if (projectManagement->IsInitialized())
    {
        if (ImGui::MenuItem("Create Scene") && GameSceneManager::Instance()->GetActiveGameScene() == nullptr)
        {
            auto newScene = MakeUnique<GameScene>();
            auto refNewScene = AssetDatabase::Instance()->Save(std::move(newScene), "./Assets/test.game");
            GameSceneManager::Instance()->SetActiveGameScene(refNewScene);
        }
        if (ImGui::MenuItem("Save"))
        {
            AssetDatabase::Instance()->SaveAll();
            projectManagement->Save();
        }

        if (ImGui::MenuItem("Reload Shader"))
        {
            AssetDatabase::Instance()->ReloadShaders();
        }
    }
    ImGui::EndMainMenuBar();
}
} // namespace Engine::Editor
