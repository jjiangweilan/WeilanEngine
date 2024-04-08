#include "GameEditor.hpp"
#include "Core/Asset.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Time.hpp"
#include "EditorState.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Inspectors/Inspector.hpp"
#include "Rendering/SurfelGI/GIScene.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "Tools/EnvironmentBaker.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace Editor
{

static std::unique_ptr<Gfx::Image> CreateImGuiFont(const char* customFont)
{
    assert(customFont == nullptr && "customFont not implemented");

    unsigned char* fontData;
    auto& io = ImGui::GetIO();
    ImFontConfig config;
    ImFont* font = nullptr;
    // if (customFont)
    // {
    //     static const ImWchar icon_ranges[] = {0x0020, 0xffff, 0};
    //     font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
    //         (std::filesystem::path(ENGINE_SOURCE_PATH) / "Resources" / "Cousine Regular Nerd Font Complete.ttf")
    //             .string()
    //             .c_str(),
    //         14,
    //         &config,
    //         icon_ranges
    //     );
    // }
    io.FontDefault = font;
    int width, height, bytePerPixel;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontData, &width, &height, &bytePerPixel);
    auto fontImage = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription((uint32_t)width, (uint32_t)height, Gfx::ImageFormat::R8G8B8A8_UNorm),
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );
    fontImage->SetName("ImGUI font");
    uint32_t fontTexSize = bytePerPixel * width * height;

    GetGfxDriver()->UploadImage(*fontImage, fontData, fontTexSize);
    return fontImage;
}

GameEditor::GameEditor(const char* path)
{
    instance = this;
    engine = std::make_unique<WeilanEngine>();
    engine->Init({.projectPath = path});
    loop = engine->CreateGameLoop();
    EditorState::gameLoop = loop;

    // engine is in another dynamic library which has different static logger instance, we need to register it for
    // editor too
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>(
        "engine logger",
        spdlog::sinks_init_list{engine->GetRingBufferLoggerSink(), consoleSink}
    );
    spdlog::set_default_logger(logger);

    auto editorConfigPath = engine->GetProjectPath() / "editorConfig.json";
    bool createEditorConfigJson = true;
    if (std::filesystem::exists(editorConfigPath))
    {
        try
        {
            editorConfig = nlohmann::json::parse(std::ifstream(editorConfigPath));
            createEditorConfigJson = false;
        }
        catch (...)
        {}
    }
    if (createEditorConfigJson)
    {
        editorConfig = nlohmann::json::object();
    }

    UUID lastActiveSceneUUID(editorConfig.value("lastActiveScene", UUID::GetEmptyUUID().ToString()));
    if (!lastActiveSceneUUID.IsEmpty())
    {
        EditorState::activeScene = (Scene*)engine->assetDatabase->LoadAssetByID(lastActiveSceneUUID);
    }

    auto& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    EnableMultiViewport();

    gameView.Init();

    fontImage = CreateImGuiFont(nullptr);
    gameEditorRenderer = std::make_unique<Editor::Renderer>(GetGfxDriver()->GetSwapChainImage(), fontImage.get());

    // toolList.emplace_back(new EnvironmentBaker());

    for (auto& t : toolList)
    {
        RegisteredTool rt;
        rt.tool = t.get();
        rt.isOpen = false;
        registeredTools.push_back(rt);
    }

    cmd = GetGfxDriver()->CreateCommandBuffer();
};

GameEditor::~GameEditor()
{
    fontImage = nullptr;
    engine->gfxDriver->WaitForIdle();
    engine->DestroyGameLoop(loop);
    loop = nullptr;
    InspectorRegistry::DestroyAll();

    if (EditorState::activeScene)
        editorConfig["lastActiveScene"] = EditorState::activeScene->GetUUID().ToString();

    if (Camera* cam = gameView.GetEditorCamera())
    {
        nlohmann::json camJson = {};
        auto pos = cam->GetGameObject()->GetPosition();
        auto rot = cam->GetGameObject()->GetRotation();
        auto scale = cam->GetGameObject()->GetScale();
        camJson["position"] = {pos.x, pos.y, pos.z};
        camJson["rotation"] = {rot.w, rot.x, rot.y, rot.z};
        camJson["scale"] = {scale.x, scale.y, scale.z};
        editorConfig["editorCamera"] = camJson;
    }

    auto editorConfigPath = engine->GetProjectPath() / "editorConfig.json";
    std::ofstream editorConfigFile(editorConfigPath);
    editorConfigFile << editorConfig.dump(0);
}

void GameEditor::SceneTree(GameObject* go, int imguiID)
{
    ImGuiTreeNodeFlags nodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (go == EditorState::selectedObject)
        nodeFlags |= ImGuiTreeNodeFlags_Selected;

    bool treeOpen = ImGui::TreeNodeEx(fmt::format("{}##{}", go->GetName(), imguiID).c_str(), nodeFlags);
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            EditorState::selectedObject = go;
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            sceneTreeContextObject = go;
            beginSceneTreeContextPopup = true;
        }
    }

    if (ImGui::BeginDragDropSource())
    {
        auto ptr = go;
        ImGui::SetDragDropPayload("game object", &ptr, sizeof(void*));

        ImGui::Text("%s", go->GetName().c_str());

        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("game object");
        if (payload && payload->IsDelivery())
        {
            GameObject* obj = *(GameObject**)payload->Data;
            if (obj != nullptr && obj != go)
            {
                obj->SetParent(go);
            }
        }

        ImGui::EndDragDropTarget();
    }

    if (treeOpen)
    {
        for (auto child : go->GetChildren())
        {
            SceneTree(child, ++imguiID);
        }
        ImGui::TreePop();
    }
}

void GameEditor::SceneTree(Scene& scene)
{
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_MenuBar);

    ImGui::BeginMenuBar();
    if (ImGui::BeginMenu("Objects"))
    {
        if (ImGui::MenuItem("Create Object"))
        {
            scene.CreateGameObject();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

    auto contentMax = ImGui::GetWindowContentRegionMax();
    auto contentMin = ImGui::GetWindowContentRegionMin();
    auto windowPos = ImGui::GetWindowPos();
    if (ImGui::BeginDragDropTargetCustom(
            {{windowPos.x + contentMin.x, windowPos.y + contentMin.y},
             {windowPos.x + contentMax.x, windowPos.y + contentMax.y}},
            999
        ))
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("game object");
        if (payload && payload->IsDelivery())
        {
            GameObject* obj = *(GameObject**)payload->Data;
            if (obj != nullptr)
            {
                // pass null to set this transform to root
                obj->SetParent(nullptr);
            }
        }

        const ImGuiPayload* objpayload = ImGui::AcceptDragDropPayload("object");
        if (objpayload && objpayload->IsDelivery())
        {
            if (Model* model = dynamic_cast<Model*>(*(Object**)objpayload->Data))
            {
                scene.AddGameObjects(model->CreateGameObject());
            }
        }

        ImGui::EndDragDropTarget();
    }

    size_t imguiTreeId = 0;
    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root, ++imguiTreeId);
    }
    ImGui::End();
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

void GameEditor::OpenSceneWindow()
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
            EditorState::activeScene = (Scene*)engine->assetDatabase->LoadAsset(fmt::format("{}.scene", openScenePath));
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

    for (auto& registeredTool : registeredTools)
    {
        auto items = registeredTool.tool->GetToolMenuItem();
        bool clicked = false;
        MenuVisitor(items.begin(), items.end(), clicked);
        if (clicked)
        {
            registeredTool.isOpen = !registeredTool.isOpen;
            if (registeredTool.isOpen)
                registeredTool.tool->Open();
            else
                registeredTool.tool->Close();
        }

        if (registeredTool.isOpen)
        {
            registeredTool.isOpen = registeredTool.tool->Tick();
            if (!registeredTool.isOpen)
            {
                registeredTool.tool->Close();
            }
        }
    }

    if (ImGui::BeginMenu("Files"))
    {
        if (ImGui::MenuItem("Save All"))
        {
            engine->assetDatabase->SaveDirtyAssets();
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
                engine->assetDatabase->SaveAsset(std::move(mat), "new material");
            }
            if (ImGui::MenuItem("Frame Graph"))
            {
                auto graph = std::make_unique<FrameGraph::Graph>();
                engine->assetDatabase->SaveAsset(std::move(graph), "new frame graph");
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
            if (EditorState::activeScene)
                engine->assetDatabase->SaveAsset(*EditorState::activeScene);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Scene Tree"))
                sceneTree = !sceneTree;
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Scene Info"))
            sceneInfo = !sceneInfo;

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::MenuItem("Assets"))
            assetWindow = !assetWindow;
        if (ImGui::MenuItem("Inspector"))
            inspectorWindow = !inspectorWindow;
        if (ImGui::MenuItem("Surfel GI Baker"))
            surfelGIBaker = !surfelGIBaker;

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void GameEditor::Start()
{
    while (true)
    {
        if (engine->BeginFrame())
        {
            if (engine->event->GetWindowClose().state)
            {
                return;
            }

            GUIPass();

            auto sceneImage = gameView.GetSceneImage();
            const Gfx::RG::ImageIdentifier* gameOutputImage = nullptr;
            if (EditorState::activeScene)
                loop->SetScene(*EditorState::activeScene);
            gameOutputImage = loop->Tick(*sceneImage);

            cmd->Reset(true);
            Render(*cmd, gameOutputImage);

            GetGfxDriver()->ExecuteCommandBuffer(*cmd);

            engine->EndFrame();
        }
    }
}

void GameEditor::GUIPass()
{
    ImGui::DockSpaceOverViewport();

    MainMenuBar();
    OpenSceneWindow();

    AssetWindow();
    InspectorWindow();
    SurfelGIBakerWindow();
    gameView.Tick();

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_R))
    {
        engine->assetDatabase->RequestShaderRefresh(false);
    }

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        engine->assetDatabase->SaveDirtyAssets();
        if (EditorState::activeScene)
            engine->assetDatabase->SaveAsset(*EditorState::activeScene);
        SPDLOG_INFO("project saved");
    }

    if (EditorState::activeScene)
    {
        if (EditorState::activeScene)
            SceneTree(*EditorState::activeScene);

        if (beginSceneTreeContextPopup)
        {
            beginSceneTreeContextPopup = false;
            ImGui::OpenPopup("GameObject Context Menu");
        }

        if (ImGui::BeginPopup("GameObject Context Menu"))
        {
            if (ImGui::Button("Delete"))
            {
                EditorState::activeScene->DestroyGameObject(sceneTreeContextObject);
                ImGui::CloseCurrentPopup();
                sceneTreeContextObject = nullptr;
            }
            ImGui::EndPopup();
        }
    }

    ConsoleOutputWindow();
}

void GameEditor::SurfelGIBakerWindow()
{
    if (surfelGIBaker)
    {
        ImGui::Begin("Surfel GI Baker");
        static SurfelGI::BakerConfig c{
            .scene = EditorState::activeScene,
            .templateShader = nullptr,
            .worldBoundsMin = {-5, -5, -5},
            .worldBoundsMax = {5, 5, 5},
        };

        const char* templateShaderName = "Template Scene Shader";
        if (c.templateShader != nullptr)
        {
            templateShaderName = c.templateShader->GetName().c_str();
        }
        if (ImGui::Button(templateShaderName))
        {
            EditorState::selectedObject = c.templateShader;
        }
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                Object* obj = *(Object**)payload->Data;
                if (Shader* shader = dynamic_cast<Shader*>(obj))
                {
                    c.templateShader = shader;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::InputFloat3("World Bounds Min", &c.worldBoundsMin[0]);
        ImGui::InputFloat3("World Bounds Max", &c.worldBoundsMax[0]);

        if (ImGui::Button("Bake"))
        {
            SurfelGI::GISceneBaker baker;
            auto giScene = std::make_unique<SurfelGI::GIScene>(baker.Bake(c));

            engine->assetDatabase->SaveAsset(std::move(giScene), "demo");
        }

        ImGui::End();
    }
}

void GameEditor::Render(Gfx::CommandBuffer& cmd, const Gfx::RG::ImageIdentifier* gameImage)
{
    // make sure we don't have sync issue with game rendering

    for (auto& t : registeredTools)
    {
        if (t.isOpen)
            t.tool->Render(cmd);
    }

    if (gameImage)
        gameView.Render(cmd, gameImage);

    ImGui::Render();
    gameEditorRenderer->Execute(ImGui::GetDrawData(), cmd);

    auto& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void GameEditor::OpenWindow() {}

void GameEditor::InspectorWindow()
{
    if (inspectorWindow)
    {
        ImGui::Begin("Inspector", &inspectorWindow, ImGuiWindowFlags_MenuBar);
        static bool lockWindow;
        static Object* primarySelected;

        if (ImGui::Checkbox("Lock window", &lockWindow))
        {
            if (lockWindow)
                primarySelected = EditorState::selectedObject;
            else
                EditorState::selectedObject = primarySelected;
        }

        if (EditorState::selectedObject)
        {
            if (!lockWindow)
                primarySelected = EditorState::selectedObject;

            if (primarySelected)
            {
                bool noInspector = primaryInspector == nullptr;
                bool chageInspector = !noInspector && primaryInspector->GetTarget() != primarySelected;
                if (noInspector || chageInspector)
                {
                    primaryInspector = InspectorRegistry::GetInspector(*primarySelected);
                    primaryInspector->OnEnable(*primarySelected);
                }

                if (primaryInspector)
                    primaryInspector->DrawInspector(*this);
            }
        }

        ImGui::End();

        if (lockWindow)
        {
            ImGui::Begin("Secondary Inspector", &lockWindow, ImGuiWindowFlags_MenuBar);

            if (EditorState::selectedObject)
            {
                bool noInspector = secondaryInspector == nullptr;
                bool chageInspector = !noInspector && secondaryInspector->GetTarget() != EditorState::selectedObject;
                if (noInspector || chageInspector)
                {
                    secondaryInspector = InspectorRegistry::GetInspector(*EditorState::selectedObject);
                    secondaryInspector->OnEnable(*EditorState::selectedObject);
                }

                if (secondaryInspector)
                    secondaryInspector->DrawInspector(*this);
            }

            ImGui::End();

            // recover selected object
            if (lockWindow == false)
            {
                EditorState::selectedObject = primarySelected;
            }
        }
    }
}

void GameEditor::AssetShowDir(const std::filesystem::path& path)
{
    for (auto entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            auto dir = std::filesystem::relative(entry.path(), path);
            bool treeOpen = ImGui::TreeNode(dir.string().c_str());
            if (treeOpen)
            {
                AssetShowDir(entry.path());
                ImGui::TreePop();
            }
        }
    }

    bool changeFileName = false;
    static std::filesystem::path changeFileNameTarget;
    for (auto entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            std::string pathStr = entry.path().filename().string();
            bool open = ImGui::TreeNodeEx(pathStr.c_str(), ImGuiTreeNodeFlags_Leaf);
            if (ImGui::BeginPopupContextItem("asset window context menu"))
            {
                if (ImGui::Selectable("Change File Name"))
                {
                    changeFileName = true;
                    changeFileNameTarget =
                        std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory());
                }
                ImGui::EndPopup();
            }
            if (open)
            {
                if (ImGui::BeginDragDropSource())
                {
                    Asset* asset = engine->assetDatabase->LoadAsset(
                        std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory())
                    );
                    ImGui::SetDragDropPayload("object", &asset, sizeof(void*));

                    ImGui::Text("%s", pathStr.c_str());

                    ImGui::EndDragDropSource();
                }
                else if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    Asset* asset = engine->assetDatabase->LoadAsset(
                        std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory())
                    );
                    if (asset)
                    {
                        EditorState::selectedObject = asset;
                    }
                }

                ImGui::TreePop();
            }
        }
    }

    static char fn[1024];
    if (changeFileName)
    {
        ImGui::OpenPopup("Change File Name");
        strcpy(fn, changeFileNameTarget.string().c_str());
    }
    if (ImGui::BeginPopupModal("Change File Name"))
    {
        ImGui::InputText("File Name: ", fn, 1024);

        if (ImGui::Selectable("Confirm"))
        {
            AssetDatabase::Singleton()->ChangeAssetPath(changeFileNameTarget, fn);
        }
        if (ImGui::Selectable("Chancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GameEditor::AssetWindow()
{
    if (assetWindow)
    {
        ImGui::Begin("Assets", &assetWindow);
        if (ImGui::TreeNode("_engine_internal_asset"))
        {
            auto& internalAssets = engine->assetDatabase->GetInternalAssets();
            for (AssetData* internalAsset : internalAssets)
            {
                auto path = std::filesystem::relative(internalAsset->GetAssetPath(), "_engine_internal/").string();
                if (ImGui::TreeNodeEx(path.c_str(), ImGuiTreeNodeFlags_Leaf))
                {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        Asset* asset = engine->assetDatabase->LoadAsset(internalAsset->GetAssetPath());
                        ImGui::SetDragDropPayload("object", &asset, sizeof(void*));

                        ImGui::Text("%s", internalAsset->GetAssetPath().string().c_str());

                        ImGui::EndDragDropSource();
                    }
                    else if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        Asset* asset = engine->assetDatabase->LoadAsset(internalAsset->GetAssetPath());
                        if (asset)
                        {
                            EditorState::selectedObject = asset;
                        }
                    }

                    if (ImGui::IsItemHovered())
                    {
                        if (ImGui::BeginTooltip())
                        {
                            ImGui::Text("%s", path.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
        AssetShowDir(engine->GetProjectPath() / "Assets");
        ImGui::End();
    }
}

void GameEditor::ConsoleOutputWindow()
{
    auto ringBufferSink = engine->GetRingBufferLoggerSink();
    auto lastRaw = ringBufferSink->last_raw();
    static auto formatter = std::make_unique<spdlog::pattern_formatter>();
    ImGui::Begin("Console");
    for (auto r = lastRaw.rbegin(); r != lastRaw.rend(); r++)
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
    //     // Note: SDL 2.0.6+ has a SDL_WINDOW_SKIP_TASKBAR flag which is supported under Windows but the way it create
    //     the
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

void GameEditor::EnableMultiViewport()
{
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
    // platform_io.Renderer_SwapBuffers = ImGui_GfxDriver_SwapBuffers;

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
}

GameEditor* GameEditor::instance = nullptr;
Object* EditorState::selectedObject = nullptr;
Scene* EditorState::activeScene = nullptr;
GameLoop* EditorState::gameLoop = nullptr;
} // namespace Editor
