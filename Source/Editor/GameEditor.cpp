#include "Editor/GameEditor.hpp"
#include "Editor/EditorConfig.hpp"
#include "Editor/EditorGUI.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/GameEditor_AssetDatabaseDebug.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Editor/NavDataAssetUtility.hpp"
#include "Editor/TerrainConfigAssetUtility.hpp"
#include "Editor/Tools/GrassSurfacePaintTool.hpp"
#include "Editor/Tools/TerrainPaintTool.hpp"
#include "Editor/Windows/CursorAtlasEditorWindow.hpp"
#include "Editor/Windows/GrassSurfacePaintWindow.hpp"
#include "Editor/Windows/TerrainPaintWindow.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/Platform/FileExplore.hpp"
#include "Engine/MiddleLayer/EngineDebug.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/Object/Component/Terrain.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"
#include "Engine/Runtime/System/Rendering/Tools/BRDFResponseGeneration.hpp"
#include "Engine/Runtime/System/UserInterface/UI.hpp"
#include "Engine/ThirdParty/imgui/implot.h"
#include "Engine/WeilanEngine.hpp"
#include "FileIcons.hpp"
#include <cmath>
#include <glm/gtx/matrix_decompose.hpp>
#include <memory>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace Editor
{

static void TrackInspectorUndoTarget(UndoManager& undoManager, Object* object)
{
    if (object == nullptr)
        return;

    if (GameObject* gameObject = dynamic_cast<GameObject*>(object))
    {
        undoManager.TrackGameObject(gameObject);
        return;
    }

    if (Component* component = dynamic_cast<Component*>(object))
    {
        undoManager.TrackGameObject(component->GetGameObject());
        return;
    }

    if (Asset* asset = dynamic_cast<Asset*>(object))
    {
        undoManager.TrackAsset(asset);
    }
}

static bool InspectorUndoInputEvent()
{
    const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    const bool popupOpen = ImGui::IsPopupOpen((ImGuiID)0, ImGuiPopupFlags_AnyPopupId);
    if (!windowHovered && !windowFocused && !popupOpen)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    return ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
           ImGui::IsMouseClicked(ImGuiMouseButton_Right) || io.InputQueueCharacters.Size > 0 ||
           ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
           ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete);
}

static bool InspectorManagesOwnUndo(Object* object)
{
    return dynamic_cast<GameObject*>(object) != nullptr || dynamic_cast<TerrainConfig*>(object) != nullptr;
}

static AssetPath GetInspectorAssetPath(InspectorBase* inspector)
{
    if (inspector == nullptr)
        return {};

    Asset* asset = dynamic_cast<Asset*>(inspector->GetTarget());
    if (asset == nullptr)
        return {};

    const AssetPath& path = AssetDatabase::Singleton()->GetAssetPath(asset->GetUUID());
    if (path.empty() || path.IsInternal())
        return {};

    return path;
}

static void DrawInspectorWindowMenuBar(GameEditor& editor, InspectorBase* inspector)
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::MenuItem("Back", nullptr, false, EditorState::CanSelectPreviousObject()))
    {
        EditorState::SelectPreviousObject();
    }

    AssetPath assetPath = GetInspectorAssetPath(inspector);
    if (ImGui::MenuItem("Pin in Browser", nullptr, false, !assetPath.empty()))
    {
        editor.PinAssetInBrowser(assetPath);
    }

    if (inspector != nullptr)
        inspector->DrawMenuBar(editor);

    ImGui::EndMenuBar();
}

static std::unique_ptr<Gfx::Image> CreateImGuiFont(const char* customFont)
{
    ASSERT(customFont == nullptr && "customFont not implemented");

    unsigned char* fontData;
    auto& io = ImGui::GetIO();
    ImFontConfig config;
    ImFont* font = nullptr;
    {
        static const ImWchar icon_ranges[] = {0x0020, 0xffff, 0};
        font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            (std::filesystem::path(ENGINE_SOURCE_PATH) / "Resources" / "MononokiNerdFont-Regular.ttf").string().c_str(),
            16,
            &config,
            icon_ranges
        );
    }
    io.FontDefault = font;
    int width, height, bytePerPixel;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontData, &width, &height, &bytePerPixel);
    auto fontImage = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription((uint32_t)width, (uint32_t)height, Gfx::GfxFormat::R8G8B8A8_UNorm),
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );
    fontImage->SetName("ImGUI font");
    uint32_t fontTexSize = bytePerPixel * width * height;

    GetGfxDriver()->UploadImage(*fontImage, fontData, fontTexSize);
    return fontImage;
}

void GameEditor::SimulatePlayerView(bool enable)
{
#if defined(_WIN32) || defined(_WIN64)
    // engine->WindowBorderless(enable);
    hideDevTool = enable;

    // adjust system window to current view size
    gameView->SetGameViewOnly(hideDevTool);
    glm::ivec2 gameResolution = gameView->GetGameScreenResolution();

    if (enable)
    {
        cacheSystemWindowSize = engine->GetSystemWindowSize();
        engine->SetSystemWindowSize(gameResolution);
        engine->PresentGameOnly(true, gameResolution);
    }
    else
    {
        engine->SetSystemWindowSize(cacheSystemWindowSize);
        engine->PresentGameOnly(false, cacheSystemWindowSize);
    }
#else
    (void)enable;
#endif
}

GameEditor::GameEditor(WeilanEngine* engine, const char* path)
{
    instance = this;
    this->engine = engine;
    EditorState::GetGameLoop() = engine->GetGameLoop();
    auto& editorConfig = EditorConfig::GetInstance();
    editorConfig.Reload();

    // engine is in another dynamic library which has different static logger instance, we need to register it for
    // editor too
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>(
        "engine logger",
        spdlog::sinks_init_list{engine->GetRingBufferLoggerSink(), consoleSink}
    );
    spdlog::set_default_logger(logger);

    this->imguiInitPath = (engine->GetProjectPath() / "imgui.ini").string();
    auto editorConfigPath = engine->GetProjectPath() / "editorState.json";
    if (std::filesystem::exists(editorConfigPath))
    {
        try
        {
            editorState = nlohmann::json::parse(std::ifstream(editorConfigPath));
        }
        catch (...)
        {
            editorState = nlohmann::json::object();
        }
    }

    if (editorState.is_null())
    {
        editorState = nlohmann::json::object();
    }

    if (!std::filesystem::exists(imguiInitPath))
    {
        std::filesystem::copy_file(
            std::filesystem::path(ENGINE_SOURCE_PATH) / "Resources" / "imgui.ini",
            imguiInitPath,
            std::filesystem::copy_options::none
        );
    }

    // Initialize standalone window implementations
    gameView = std::make_unique<GameView>();
    sceneEditor = std::make_unique<SceneEditor>();
    assetBrowser = std::make_unique<AssetBrowser>(engine, this);
    gizmoManager = std::make_unique<GizmoManager>();
    engineCommandGUI = std::make_unique<EngineCommandGUI>();
    assetDatabaseDebug = std::make_unique<GameEditorAssetDatabaseDebug>();

    gameView->Init();

    // Load previous active scene, we need gameView to init UI first
    UUID lastActiveSceneUUID(editorState.value("lastActiveScene", UUID::GetEmptyUUID().ToString()));
    if (!lastActiveSceneUUID.IsEmpty())
    {
        auto scene = (Scene*)engine->assetDatabase->LoadScene(lastActiveSceneUUID);
        if (scene)
        {
            SceneManager::SetActiveScene(scene);
            EditorState::GetGameLoop()->SetScene(*scene);
        }
    }

    editorContext->SetGizmoManager(gizmoManager.get());
    gizmoManager->SetEditorContext(editorContext.get());
    sceneEditor->Init(editorContext.get());
    sceneEditor->LoadEditorState(editorState);

    // Configure ImGui.io
    auto& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = imguiInitPath.c_str();

    fontImage = CreateImGuiFont(nullptr);
    gameEditorRenderer = std::make_unique<Editor::Renderer>(GetGfxDriver()->GetSwapChainImage(), fontImage.get());
    FileIcons::Instance().Initialize(engine);

    ImPlot::CreateContext();
};

GameEditor::~GameEditor()
{
    // SaveProject();

    ImPlot::DestroyContext();
    fontImage = nullptr;
    engine->gfxDriver->WaitForIdle();
    FileIcons::Instance().Shutdown();
    InspectorRegistry::DestroyAll();

    if (SceneManager::GetActiveScene())
        editorState["lastActiveScene"] = SceneManager::GetActiveScene()->GetUUID().ToString();

    sceneEditor->SaveEditorState(editorState);

    auto gameViewResolution = gameView->GetGameScreenResolution();
    editorState["gameView"]["resolution"] = {gameViewResolution.x, gameViewResolution.y};
    editorState["gameView"]["resolutionSelectionIndex"] = gameView->GetGameScreenResolutionSelectionIndex();

    auto editorStatePath = engine->GetProjectPath() / "editorState.json";
    std::ofstream editorStateFile(editorStatePath);
    editorStateFile << editorState.dump(0);

    // cleanup editor state
    EditorState::Clear();
}

static void ProfileTree(const ProfileScope& scope, int id)
{
    ImGui::PushID(id);
    ImGuiTreeNodeFlags flags = scope.children.empty() ? ImGuiTreeNodeFlags_Leaf : 0;
    if (ImGui::TreeNodeEx("#ProfileTree", flags, "%s - %f", scope.label.c_str(), scope.GetMilliseconds()))
    {
        for (auto& child : scope.children)
        {
            ProfileTree(*child, ++id);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void GameEditor::ShowGameProfiler(IProfiler& cpuProfiler)
{

    // auto& gpuProfiles = GetGfxDriver()->GetFrameProfiles();
    ImGui::Begin("Profiler Module");

    if (ImGui::IsWindowCollapsed())
    {
        cpuProfiler.Pause();
        GetGfxDriver()->GetGPUProfiler().Pause();
        GetGfxDriver()->SetGPUProfilerEnabled(false);
    }

    ImGui::Text("Frame Profiler:");

    static int selectedFrame = 0;
    static int actuallySelectedFrame = 0;
    static bool cpuOrGpu = true;

    auto& gpuProfiler = GetGfxDriver()->GetGPUProfiler();
    auto& profiler = cpuOrGpu ? cpuProfiler : gpuProfiler;
    auto& frameProfiles = cpuOrGpu ? cpuProfiler.GetFrameProfiles() : gpuProfiler.GetFrameProfiles();
    const char* profileName = cpuOrGpu ? "CPU" : "GPU";

    if (ImGui::Button(profileName))
    {
        cpuOrGpu = !cpuOrGpu;
        if (cpuOrGpu)
        {
            cpuProfiler.Resume();
            gpuProfiler.Pause();
        }
        else
        {
            gpuProfiler.Resume();
            cpuProfiler.Pause();
        }

        GetGfxDriver()->SetGPUProfilerEnabled(!cpuOrGpu);
    }

    ImGui::SameLine();
    if (profiler.IsPaused())
    {
        if (ImGui::Button("Resume"))
            profiler.Resume();
    }
    else
    {
        if (ImGui::Button("Pause"))
        {
            profiler.Pause();
        }
    }

    ImPlot::SetNextAxisLimits(ImAxis_X1, 0, Profiler::MAX_FRAME_TRACKED);
    ImPlot::SetNextAxisLimits(ImAxis_Y1, 0, 16);

    if (ImPlot::BeginPlot("Frame Profiles", ImVec2(-1, 300)))
    {
        ImPlotAxisFlags xAxesFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines;
        ImPlot::SetupAxes(nullptr, nullptr, xAxesFlags, 0);
        auto selectedPos = ImPlot::GetPlotMousePos();
        if (!frameProfiles.empty())
        {
            std::vector<float> frameTimes = profiler.GetFlattendFrametime();
            ImPlot::PlotLine(profileName, frameTimes.data(), frameTimes.size(), 1, 0);
            if (profiler.IsPaused())
                ImPlot::PlotInfLines("selected", &selectedFrame, 1);
            else if (ImGui::IsWindowHovered())
                ImPlot::PlotInfLines("selected", &selectedPos.x, 1);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            selectedFrame = std::round(selectedPos.x);
            actuallySelectedFrame =
                profiler.GetTrackCycles() == 0
                    ? selectedFrame
                    : (selectedFrame + (profiler.GetLatestFrameIndex() + 1)) % Profiler::MAX_FRAME_TRACKED;
        }

        ImPlot::EndPlot();
    }

    if (profiler.IsPaused())
    {
        if (actuallySelectedFrame >= 0 && actuallySelectedFrame < frameProfiles.size())
        {
            auto& frameProfile = frameProfiles[actuallySelectedFrame];
            if (frameProfile != nullptr)
            {
                ProfileTree(*frameProfiles[actuallySelectedFrame], 0);
            }
        }
    }

    ImGui::End();
}

GameObject* GameEditor::AddPrimitiveAssetToScene(Scene& scene, std::string_view path)
{
    auto model = dynamic_cast<Model*>(AssetDatabase::Singleton()->LoadAsset(path));
    if (model == nullptr || model->GetMeshes().empty() || model->GetMeshes()[0] == nullptr)
    {
        spdlog::error("Failed to create primitive from asset: {}", path);
        return nullptr;
    }

    auto gameObject = std::make_unique<GameObject>();
    gameObject->SetName(model->GetName());
    gameObject->SetWantsToBeEnabled();

    auto meshRenderer = gameObject->AddComponent<MeshRenderer>();
    meshRenderer->SetMesh(model->GetMeshes()[0].get());
    meshRenderer->SetMaterial(EngineInternalResources::GetDefaultGridMaterial());
    gameObject->AddComponent<PhysicsBody>();

    gameObject->SetName("New GameObject");

    return scene.AddGameObject(std::move(gameObject));
}

static void MenuVisitor(std::vector<std::string>::iterator iter, std::vector<std::string>::iterator end, bool& clicked)
{
    if (iter == end)
    {
        return;
    }
    else if (iter == end - 1)
    {
        if (ImGui::MenuItem(iter->c_str()))
        {
            clicked = true;
        }
        return;
    }

    if (ImGui::BeginMenu(iter->c_str()))
    {
        iter += 1;
        MenuVisitor(iter, end, clicked);
        ImGui::EndMenu();
    }
}

void GameEditor::ShowSceneWindow()
{
    if (openSceneWindow)
    {
        ImGui::Begin(
            "Open Scene",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
        );

        static char openScenePath[1024];
        ImGui::InputText("Path", openScenePath, 1024);
        if (ImGui::Button("Open"))
        {
            SetActiveScene((Scene*)engine->assetDatabase->LoadAsset(fmt::format("{}.scene", openScenePath)));
            openSceneWindow = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            openSceneWindow = false;
        }

        ImGui::End();
    }

    if (createSceneWindow)
    {
        ImGui::Begin(
            "Create Scene",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
        );

        static char createScenePath[1024];
        ImGui::InputText("path", createScenePath, 1024);
        if (ImGui::Button("Create"))
        {
            auto scene = std::make_unique<Scene>();
            engine->assetDatabase->SaveAsset(std::move(scene), createScenePath);
            createSceneWindow = false;
        }
        if (ImGui::Button("Close"))
        {
            openSceneWindow = false;
        }

        ImGui::End();
    }
}

void GameEditor::MainMenuBar()
{
    ImGui::BeginMainMenuBar();

    if (ImGui::BeginMenu("Files"))
    {
        if (ImGui::MenuItem("Save All"))
        {
            engine->assetDatabase->SaveDirtyAssets();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        auto& undoManager = EditorState::GetUndoManager();
        const std::string undoLabel =
            undoManager.CanUndo() ? fmt::format("Undo {}", undoManager.GetUndoName()) : "Undo";
        const std::string redoLabel =
            undoManager.CanRedo() ? fmt::format("Redo {}", undoManager.GetRedoName()) : "Redo";
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, undoManager.CanUndo()))
        {
            undoManager.Undo();
        }
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y / Ctrl+Shift+Z", false, undoManager.CanRedo()))
        {
            undoManager.Redo();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Assets"))
    {
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Scene"))
            {
                createSceneWindow = !createSceneWindow;
            }
            if (ImGui::MenuItem("Material"))
            {
                auto mat = std::make_unique<Material>();
                engine->assetDatabase->SaveAsset(std::move(mat), "New Material");
            }
            if (ImGui::MenuItem("Render Pipeline Setting"))
            {
                auto renderPipelineSetting = std::make_unique<Rendering::RenderPipelineSetting>();
                engine->assetDatabase->SaveAsset(std::move(renderPipelineSetting), "New RenderPipelineSetting");
            }
            if (ImGui::MenuItem("Nav Data"))
            {
                CreateNavDataAsset(*engine->assetDatabase, "New NavData");
            }
            if (ImGui::MenuItem("Terrain Config"))
            {
                CreateTerrainConfigAsset(*engine->assetDatabase, "New Terrain");
            }
            if (ImGui::MenuItem("Binary Asset"))
            {
                engine->assetDatabase->SaveAsset(std::make_unique<BinaryAsset>(), "New Binary Asset");
            }
            if (ImGui::MenuItem("Cursor Atlas"))
            {
                activeWindows.push_back(std::make_unique<CursorAtlasEditorWindow>());
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Refresh Shaders"))
        {
            engine->assetDatabase->RequestShaderRefresh();
        }
        if (ImGui::MenuItem("Open Scene"))
        {
            openSceneWindow = !openSceneWindow;
        }
        if (ImGui::MenuItem("Save Scene"))
        {
            if (SceneManager::GetActiveScene())
                engine->assetDatabase->SaveAsset(*SceneManager::GetActiveScene());
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Scene Tree"))
            sceneTree = !sceneTree;

        if (ImGui::MenuItem("Scene Info"))
            sceneInfo = !sceneInfo;

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        std::tuple<const char*, const char*, bool&> windowToggles[] = {
            {"Assets", nullptr, assetWindow},
            {"Inspector", "Ctrl+I", inspectorWindow},
            {"Surfel GI Baker", nullptr, surfelGIBaker},
            {"AssetDatabase", nullptr, assetDatabaseWindow},
            {"PBR Baker", nullptr, pbrBaker}

        };
        for (auto& w : windowToggles)
        {
            if (ImGui::MenuItem(std::get<0>(w), std::get<1>(w)))
            {
                std::get<2>(w) = !std::get<2>(w);
            }
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug"))
    {
        if (ImGui::MenuItem("AssetDatabase", nullptr, debugEngineResources))
        {
            debugEngineResources = !debugEngineResources;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project"))
    {
        if (ImGui::MenuItem("Open Project Folder"))
        {
            Platform::FileExplore::OpenFolder(AssetDatabase::Singleton()->GetProjectRoot());
        }
        if (ImGui::MenuItem("Open Engine Root Path"))
        {
            Platform::FileExplore::OpenFolder(".");
        }
        ImGui::EndMenu();
    }

    bool isRenderDocInitialized = GetGfxDriver()->IsRenderDocInitialized();
    if (ImGui::MenuItem(isRenderDocInitialized ? "RenderDoc(Capture)" : "RenderDoc"))
    {
        if (!isRenderDocInitialized)
            GetGfxDriver()->InitializeRenderDoc();

        if (isRenderDocInitialized)
        {
            GetGfxDriver()->CaptureFrameRenderDoc(true);
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    if (ImGui::MenuItem(hideDevTool ? "Show Dev Tool" : "Hide Dev Tool"))
    {
        SimulatePlayerView(!hideDevTool);
    }
#endif

    for (auto& windowInfo : WindowRegistery::GetRegistery())
    {
        WindowRegisteryIteration(windowInfo, 0);
    }

    ImGui::EndMainMenuBar();
}

void GameEditor::GUIPass()
{
    // gizmo states needs to be reset as nearly as possible to that calls to mark gizmo actived can be correctly set

    if (!hideDevTool)
    {
        ENGINE_BEGIN_PROFILE("GUI - Dev Tools Prepass")
        sceneEditor->ResetGizmoState();

        ImGui::DockSpaceOverViewport();

        MainMenuBar();

        ShowSceneWindow();
        assetBrowser->Show(assetWindow);
        ShowInspectorWindow();
        ShowSurfelGIBakerWindow();
        ShowRenderPipelineSetting();
        ShowStaticEngineDebugs();

        ShowEngineResourceDebug();

        std::vector<std::unique_ptr<Window>*> toClose;
        for (auto& w : activeWindows)
        {
            if (!w->Tick())
            {
                toClose.push_back(&w);
                w->OnClose();
            }
        }
        for (auto close : toClose)
        {
            activeWindows.remove(*close);
        }
        ENGINE_END_PROFILE; // GUI - Dev Tools Prepass
    }

    ENGINE_BEGIN_PROFILE("GUI - Game View")
    gameView->Tick();
    ENGINE_END_PROFILE; // GUI - Game View

    if (!hideDevTool)
    {
        ENGINE_BEGIN_PROFILE("GUI - Dev Tools Postpass")
        sceneEditor->Tick();
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_R))
        {
            engine->assetDatabase->RequestShaderRefresh(false);
            engine->assetDatabase->ReloadScripts();
            EditorConfig::GetInstance().Reload();
        }

        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            SaveProject();
            SPDLOG_INFO("project saved");
        }

        if (!ImGui::GetIO().WantTextInput)
        {
            auto& undoManager = EditorState::GetUndoManager();
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
            {
                undoManager.Undo();
            }
            else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
            {
                undoManager.Redo();
            }
        }

        if (SceneManager::GetActiveScene())
        {
            ShowSceneTree(*SceneManager::GetActiveScene());
        }

        ENGINE_BEGIN_PROFILE("GUI - Profiler Window")
        ShowGameProfiler(Profiler::GetSingleton());
        ENGINE_END_PROFILE; // GUI - Profiler Window

        ENGINE_BEGIN_PROFILE("GUI - Console Window")
        ShowConsoleOutputWindow();
        ENGINE_END_PROFILE; // GUI - Console Window

        ENGINE_BEGIN_PROFILE("GUI - Asset Database Window")
        ShowAssetDatabaseViewer();
        ENGINE_END_PROFILE; // GUI - Asset Database Window

        ENGINE_BEGIN_PROFILE("GUI - Engine Command Window")
        engineCommandGUI->EditorDraw();
        ENGINE_END_PROFILE; // GUI - Engine Command Window

        if (pbrBaker)
        {
            ImGui::Begin("PBR Baker", &pbrBaker);

            if (ImGui::Button("Bake"))
            {
                Rendering::GenerateBRDFResponseTexture(
                    (AssetDatabase::Singleton()->GetProjectRoot() / "Assets/PBRResponse.ktx").string().c_str()
                );
            }

            ImGui::End();
        }
        ENGINE_END_PROFILE; // GUI - Dev Tools Postpass
    }

    // Configure for first frame
    static bool firstFrame = true;
    if (firstFrame)
    {
        // Set focus window
        ImGui::SetWindowFocus(assetBrowser->GetWindowName());
        firstFrame = false;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_H))
    {
        SimulatePlayerView(!hideDevTool);
    }
#endif

    if (!ImGui::IsAnyItemActive())
        EditorState::GetUndoManager().CommitImplicitTransaction();
}

void GameEditor::ShowSurfelGIBakerWindow()
{
    if (surfelGIBaker)
    {}
}

void GameEditor::Render(
    Gfx::CommandBuffer& cmd, const Gfx::ImageIdentifier* gameImage, const Gfx::ImageIdentifier* gameDepthImage
)
{
    ENGINE_BEGIN_PROFILE("ImGui Render");
    ImGui::Render();
    ENGINE_END_PROFILE; // ImGui Render

    ENGINE_BEGIN_PROFILE("Render");
    //
    // make sure we don't have sync issue with game rendering

    glm::float4 color = {0.3, 0.6, 0.12, 1.0};
    cmd.BeginLabel("Editor", &color[0]);

    ENGINE_BEGIN_PROFILE("Scene Editor")
    sceneEditor->Render(cmd);
    ENGINE_END_PROFILE; // Scene Editor

    ENGINE_BEGIN_PROFILE("Game View")
    if (gameImage)
    {
        gameView->Render(cmd, gameImage, gameDepthImage);
    }
    ENGINE_END_PROFILE; // Game View

    ENGINE_BEGIN_PROFILE("Editor")
    FileIcons::Instance().RenderQueuedPreviews(cmd);
    gameEditorRenderer->Execute(ImGui::GetDrawData(), cmd);
    ENGINE_END_PROFILE; // Editor

    auto& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    cmd.EndLabel();
    ENGINE_END_PROFILE; // Render
}

void GameEditor::OpenWindow() {}

void GameEditor::SetActiveSceneEditorTool(SceneEditorTool* tool)
{
    if (sceneEditor)
        sceneEditor->SetActiveTool(tool);
}

void GameEditor::OpenGrassSurfacePaintWindow(GrassSurface* gs)
{
    GrassSurfacePaintWindow* existing = nullptr;
    for (auto& w : activeWindows)
    {
        existing = dynamic_cast<GrassSurfacePaintWindow*>(w.get());
        if (existing)
            break;
    }

    if (existing)
    {
        existing->SetTargetGrassSurface(gs);
    }
    else
    {
        auto w = std::unique_ptr<GrassSurfacePaintWindow>(new GrassSurfacePaintWindow());
        w->SetTargetGrassSurface(gs);
        w->OnOpen();
        existing = w.get();
        activeWindows.push_back(std::move(w));
    }

    if (auto* tool = existing->GetTool())
    {
        SetActiveSceneEditorTool(tool);
        existing->SetToolActive(true);
    }
}

void GameEditor::OpenTerrainPaintWindow(Terrain* terrain)
{
    TerrainPaintWindow* existing = nullptr;
    for (auto& window : activeWindows)
    {
        existing = dynamic_cast<TerrainPaintWindow*>(window.get());
        if (existing != nullptr)
            break;
    }

    if (existing != nullptr)
    {
        existing->SetTargetTerrain(terrain);
    }
    else
    {
        auto window = std::unique_ptr<TerrainPaintWindow>(new TerrainPaintWindow());
        window->SetTargetTerrain(terrain);
        window->OnOpen();
        existing = window.get();
        activeWindows.push_back(std::move(window));
    }

    SetActiveSceneEditorTool(existing->GetTool());
    existing->SetToolActive(true);
}

void GameEditor::DrawInspectorWithUndo(Object* object, InspectorBase* inspector)
{
    if (object == nullptr || inspector == nullptr)
        return;

    if (InspectorManagesOwnUndo(object))
    {
        inspector->DrawInspector(*this);
        return;
    }

    auto& undoManager = EditorState::GetUndoManager();
    if (InspectorUndoInputEvent())
        TrackInspectorUndoTarget(undoManager, object);

    inspector->DrawInspector(*this);
}

void GameEditor::PinAssetInBrowser(const AssetPath& path)
{
    assetBrowser->PinAsset(path);
    assetWindow = true;
}

void GameEditor::ShowInspectorWindow()
{
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_I))
    {
        inspectorWindow = !inspectorWindow;
    }

    if (inspectorWindow)
    {
        ImGui::Begin("Inspector", &inspectorWindow, ImGuiWindowFlags_MenuBar);
        static bool lockWindow;
        static ObjPtr<Object> primarySelected;

        auto selectedObject = EditorState::GetMainSelectedObject();
        if (selectedObject)
        {
            if (!lockWindow)
                primarySelected = selectedObject;

            if (primarySelected)
            {
                bool noInspector = primaryInspector == nullptr;
                bool chageInspector = !noInspector && primaryInspector->GetTarget() != primarySelected.Get();
                if (noInspector || chageInspector)
                {
                    primaryInspector = InspectorRegistry::GetInspector(*primarySelected);
                    primaryInspector->OnEnable(*primarySelected);
                }
            }
        }

        InspectorBase* primaryMenuInspector = (primarySelected && (lockWindow || selectedObject)) ? primaryInspector : nullptr;
        DrawInspectorWindowMenuBar(*this, primaryMenuInspector);

        if (ImGui::Checkbox("Lock window", &lockWindow))
        {
            if (lockWindow)
                primarySelected = EditorState::GetMainSelectedObject();
            else
                EditorState::SelectObject(primarySelected);
        }

        if (primarySelected && (lockWindow || selectedObject) && primaryInspector)
        {
            DrawInspectorWithUndo(primarySelected.Get(), primaryInspector);
        }

        ImGui::End();

        if (lockWindow)
        {
            ImGui::Begin("Secondary Inspector", &lockWindow, ImGuiWindowFlags_MenuBar);

            auto selectedObject = EditorState::GetMainSelectedObject();
            if (selectedObject)
            {
                bool noInspector = secondaryInspector == nullptr;
                bool chageInspector = !noInspector && secondaryInspector->GetTarget() != selectedObject;
                if (noInspector || chageInspector)
                {
                    secondaryInspector = InspectorRegistry::GetInspector(*selectedObject);
                    secondaryInspector->OnEnable(*selectedObject);
                }
            }

            DrawInspectorWindowMenuBar(*this, selectedObject ? secondaryInspector : nullptr);

            if (selectedObject && secondaryInspector)
            {
                DrawInspectorWithUndo(selectedObject, secondaryInspector);
            }

            ImGui::End();

            // recover selected object
            if (lockWindow == false)
            {
                EditorState::SelectObject(primarySelected);
            }
        }
    }
}

void GameEditor::ShowConsoleOutputWindow()
{
    auto ringBufferSink = engine->GetRingBufferLoggerSink();
    auto lastRaw = ringBufferSink->last_raw();
    static auto formatter = std::make_unique<spdlog::pattern_formatter>();
    static bool consoleScrollInitialized = false;
    ImGui::SetNextWindowSize({300, 300}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Console");
    const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
    for (auto r = lastRaw.begin(); r != lastRaw.end(); r++)
    {
        spdlog::memory_buf_t formatted;
        formatter->format(*r, formatted);
        bool colorPushed = false;
        if (r->level == spdlog::level::trace || r->level == spdlog::level::info || r->level == spdlog::level::debug)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0.0f, 0.8f, 0.0f).Value);
            colorPushed = true;
        }
        else if (r->level == spdlog::level::warn)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 165, 0, 1));
            colorPushed = true;
        }
        else if (r->level == spdlog::level::err || r->level == spdlog::level::critical)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
            colorPushed = true;
        }

        ImGui::TextWrapped("%s", fmt::to_string(formatted).data());

        if (colorPushed)
            ImGui::PopStyleColor();
    }
    if (!consoleScrollInitialized || wasAtBottom)
    {
        ImGui::SetScrollHereY(1.0f);
        consoleScrollInitialized = true;
    }
    ImGui::End();
}

static void ImGui_ImplSDL2_CreateWindow(ImGuiViewport* viewport)
{
    Uint32 sdl_flags = 0;
    sdl_flags |= SDL_WINDOW_VULKAN;
    sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    sdl_flags |= SDL_WINDOW_HIDDEN;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? SDL_WINDOW_BORDERLESS : 0;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? 0 : SDL_WINDOW_RESIZABLE;
#if !defined(_WIN32)
    // See SDL hack in ImGui_ImplSDL2_ShowWindow().
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon) ? SDL_WINDOW_SKIP_TASKBAR : 0;
#endif
#if SDL_HAS_ALWAYS_ON_TOP
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_TopMost) ? SDL_WINDOW_ALWAYS_ON_TOP : 0;
#endif
    SDL_Window* window = SDL_CreateWindow(
        "No Title Yet",
        (int)viewport->Pos.x,
        (int)viewport->Pos.y,
        (int)viewport->Size.x,
        (int)viewport->Size.y,
        sdl_flags
    );

    viewport->PlatformHandle = window;
}

static void ImGui_ImplSDL2_DestroyWindow(ImGuiViewport* viewport)
{
    SDL_DestroyWindow((SDL_Window*)viewport->PlatformHandle);
    viewport->PlatformHandle = nullptr;
}

static void ImGui_ImplSDL2_ShowWindow(ImGuiViewport* viewport)
{
    // #if defined(_WIN32)
    //     HWND hwnd = (HWND)viewport->PlatformHandleRaw;
    //
    //     // SDL hack: Hide icon from task bar
    //     // Note: SDL 2.0.6+ has a SDL_WINDOW_SKIP_TASKBAR flag which is supported under Windows but the way it
    //     create the
    //     // window breaks our seamless transition.
    //     if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
    //     {
    //         LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    //         ex_style &= ~WS_EX_APPWINDOW;
    //         ex_style |= WS_EX_TOOLWINDOW;
    //         ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
    //     }
    //
    //     // SDL hack: SDL always activate/focus windows :/
    //     if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
    //     {
    //         ::ShowWindow(hwnd, SW_SHOWNA);
    //         return;
    //     }
    // #endif

    SDL_ShowWindow((SDL_Window*)viewport->PlatformHandle);
}

static ImVec2 ImGui_ImplSDL2_GetWindowPos(ImGuiViewport* viewport)
{
    int x = 0, y = 0;
    SDL_GetWindowPosition((SDL_Window*)viewport->PlatformHandle, &x, &y);
    return ImVec2((float)x, (float)y);
}

static void ImGui_ImplSDL2_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    SDL_SetWindowPosition((SDL_Window*)viewport->PlatformHandle, (int)pos.x, (int)pos.y);
}

static ImVec2 ImGui_ImplSDL2_GetWindowSize(ImGuiViewport* viewport)
{
    int w = 0, h = 0;
    SDL_GetWindowSize((SDL_Window*)viewport->PlatformHandle, &w, &h);
    return ImVec2((float)w, (float)h);
}

static void ImGui_ImplSDL2_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    SDL_SetWindowSize((SDL_Window*)viewport->PlatformHandle, (int)size.x, (int)size.y);
}

static void ImGui_ImplSDL2_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    SDL_SetWindowTitle((SDL_Window*)viewport->PlatformHandle, title);
}

static void ImGui_ImplSDL2_SetWindowFocus(ImGuiViewport* viewport)
{
    SDL_RaiseWindow((SDL_Window*)viewport->PlatformHandle);
}

static bool ImGui_ImplSDL2_GetWindowFocus(ImGuiViewport* viewport)
{
    return (SDL_GetWindowFlags((SDL_Window*)viewport->PlatformHandle) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

static bool ImGui_ImplSDL2_GetWindowMinimized(ImGuiViewport* viewport)
{
    return (SDL_GetWindowFlags((SDL_Window*)viewport->PlatformHandle) & SDL_WINDOW_MINIMIZED) != 0;
}

struct GfxDriverWindowData
{
    Gfx::Window* window;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
};

static void ImGui_GfxDriver_CreateWindow(ImGuiViewport* viewport)
{
    GfxDriverWindowData* data = new GfxDriverWindowData();
    data->window = GetGfxDriver()->CreateExtraWindow((SDL_Window*)viewport->PlatformHandle);
    data->renderer =
        std::make_unique<Renderer>(data->window->GetSwapchainImage(), GameEditor::instance->fontImage.get());
    data->cmd = GetGfxDriver()->CreateCommandBuffer();
    viewport->RendererUserData = data;
}

static void ImGui_GfxDriver_DestroyWindow(ImGuiViewport* viewport)
{
    auto d = (GfxDriverWindowData*)viewport->RendererUserData;
    if (d != nullptr)
    {
        GetGfxDriver()->DestroyExtraWindow(d->window);
        delete d;
        viewport->RendererUserData = nullptr;
    }
}

static void ImGui_GfxDriver_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    GfxDriverWindowData* data = (GfxDriverWindowData*)viewport->RendererUserData;
    data->window->SetSurfaceSize(size.x, size.y);
}

static void ImGui_GfxDriver_RenderWindow(ImGuiViewport* viewport, void* render_arg)
{
    GfxDriverWindowData* data = (GfxDriverWindowData*)viewport->RendererUserData;
    data->cmd->Reset(true);
    data->renderer->Execute(viewport->DrawData, *data->cmd);
    GetGfxDriver()->ExecuteCommandBuffer(*data->cmd);
}

static void ImGui_GfxDriver_SwapBuffers(ImGuiViewport* viewport, void*)
{
    GfxDriverWindowData* data = (GfxDriverWindowData*)viewport->RendererUserData;
    data->window->Present();
}

void GameEditor::EnableMultiViewport()
{
    // bug: when there is not imgui.init and the initial window size is out of main viewport, there is an extra
    // viewport will be created, which SDL can't successfully create

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplSDL2_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplSDL2_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplSDL2_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplSDL2_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplSDL2_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplSDL2_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplSDL2_GetWindowSize;
    platform_io.Platform_SetWindowFocus = ImGui_ImplSDL2_SetWindowFocus;
    platform_io.Platform_GetWindowFocus = ImGui_ImplSDL2_GetWindowFocus;
    platform_io.Platform_GetWindowMinimized = ImGui_ImplSDL2_GetWindowMinimized;
    platform_io.Platform_SetWindowTitle = ImGui_ImplSDL2_SetWindowTitle;
    // platform_io.Platform_RenderWindow = ImGui_ImplSDL2_RenderWindow;

    platform_io.Renderer_CreateWindow = ImGui_GfxDriver_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_GfxDriver_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_GfxDriver_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_GfxDriver_RenderWindow;
    platform_io.Renderer_SwapBuffers = ImGui_GfxDriver_SwapBuffers;

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
}

void GameEditor::WindowRegisteryIteration(WindowRegisterInfo& info, int pathIndex)
{
    int maxPathIndex = info.menuPath.size() - 1;
    if (pathIndex != maxPathIndex)
    {
        if (ImGui::BeginMenu(info.menuPath[pathIndex].c_str()))
        {
            WindowRegisteryIteration(info, pathIndex + 1);
            ImGui::EndMenu();
        }
    }
    else
    {
        if (ImGui::MenuItem(info.menuPath[pathIndex].c_str()))
        {
            auto w = info.factory();
            w->OnOpen();
            activeWindows.push_back(std::move(w));
        }
    }
}

void GameEditor::ShowAssetDatabaseViewer()
{
    if (assetDatabaseWindow)
    {
        ImGui::Begin("AssetDatabase", &assetDatabaseWindow);

        auto& data = AssetDatabase::Singleton()->GetAssetData();

        for (auto& d : data)
        {
            nlohmann::json info = d->DumpInfo();
            {
                std::string assetPath = info["assetPath"];
                if (ImGui::TreeNode(assetPath.c_str()))
                {
                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();
    }
}

GameEditor* GameEditor::instance = nullptr;

void GameEditor::SaveProject()
{
    engine->assetDatabase->SaveDirtyAssets();
    if (auto scene = SceneManager::GetActiveScene())
        engine->assetDatabase->SaveAsset(*scene);
}

void GameEditor::SetActiveScene(ObjPtr<Scene> scene)
{
    EditorState::GetUndoManager().Clear();
    EditorState::SelectObject(nullptr);
    sceneEditor->SetActiveScene(scene);
}

void GameEditor::ShowEngineResourceDebug()
{
    if (!debugEngineResources)
        return;

    assetDatabaseDebug->Show(debugEngineResources);
}

void GameEditor::ShowRenderPipelineSetting()
{
    ImGui::Begin("Render Pipeline");
    auto setting = EditorState::GetGameLoop()->GetRenderPipeline().GetRenderPipelineSetting();
    EditorGUI::AutoObjectInspector(setting);
    ImGui::End();
}

void GameEditor::ShowStaticEngineDebugs()
{
    ImGui::Begin("Engine Debug", &engineDebug);
    struct StaticEngineDebugsInfo
    {
        const char* name;
        bool* value;
    };

    static std::vector<StaticEngineDebugsInfo> debugs = {
        {"Scene BVH", &EngineDebugVars::SceneBVH()},
        {"Shadow Frustum", &EngineDebugVars::ShadowFrustum()}
    };

    for (const auto& p : debugs)
    {
        ImGui::Checkbox(p.name, p.value);
    }
    ImGui::End();
}

void GameEditor::Tick()
{
    endEvents.TickBegin();
    endPopup.TickBegin();

    ENGINE_SCOPED_PROFILE("Before Game Tick")
    if (engine->event->GetWindowClose().state)
    {
        gameView->Deinit(); // stop playing the game
        sceneEditor->Deinit();
        endPopup.Show(
            "Save Project?",
            [this]()
            {
                SaveProject();
                engine->CloseEngine();
            },
            [this]()
            { engine->CloseEngine(); }
        );
    }

    ENGINE_BEGIN_PROFILE("GUI")
    GUIPass();
    ENGINE_END_PROFILE; // GUI
}

void GameEditor::AfterGameLoopTick()
{
    ENGINE_SCOPED_PROFILE("After Game Tick");
    endPopup.TickEnd();
    endEvents.TickEnd();
}

float2 GameEditor::GetGameScreenSize()
{
    auto gameScreenImage = gameView->GetGameScreenImage();
    auto screenSize = gameScreenImage->GetDescription().GetSize();
    return screenSize;
}

bool GameEditor::IsGameViewVisible()
{
    return gameView->IsVisible();
}

} // namespace Editor
  //
