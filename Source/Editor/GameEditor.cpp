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
#include "spdlog/spdlog.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <unordered_map>

namespace Editor
{
GameEditor::GameEditor(const char* path)
{
    instance = this;
    engine = std::make_unique<WeilanEngine>();
    engine->Init({.projectPath = path});
    loop = engine->CreateGameLoop();

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

    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::GetIO().ConfigFlags += ImGuiConfigFlags_DockingEnable;

    gameView.Init();

    gameEditorRenderer = std::make_unique<Editor::Renderer>();
    gameEditorRenderer->BuildGraph();

    // toolList.emplace_back(new EnvironmentBaker());

    for (auto& t : toolList)
    {
        RegisteredTool rt;
        rt.tool = t.get();
        rt.isOpen = false;
        registeredTools.push_back(rt);
    }
};

GameEditor::~GameEditor()
{
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
            if (engine->event->GetSwapchainRecreated().state)
            {
                gameEditorRenderer->BuildGraph();
            }

            GUIPass();

            loop->Tick();

            GetGfxDriver()->Schedule([this](Gfx::CommandBuffer& cmd) { Render(cmd); });

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
        engine->assetDatabase->RequestShaderRefresh(true);
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

void GameEditor::Render(Gfx::CommandBuffer& cmd)
{
    // make sure we don't have sync issue with game rendering

    for (auto& t : registeredTools)
    {
        if (t.isOpen)
            t.tool->Render(cmd);
    }

    if (EditorState::activeScene)
        gameView.Render(cmd, EditorState::activeScene);

    gameEditorRenderer->Execute(cmd);
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

    for (auto entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            std::string pathStr = entry.path().filename().string();
            if (ImGui::TreeNodeEx(pathStr.c_str(), ImGuiTreeNodeFlags_Leaf))
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
}

void GameEditor::AssetWindow()
{
    if (assetWindow)
    {
        ImGui::Begin("Assets", &assetWindow);
        AssetShowDir(engine->GetProjectPath() / "Assets");
        ImGui::End();

        ImGui::Begin("Internal Assets", &assetWindow);
        auto& internalAssets = engine->assetDatabase->GetInternalAssets();
        for (AssetData* internalAsset : internalAssets)
        {
            ImGui::Text("%s", internalAsset->GetAssetPath().string().c_str());

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
        }
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

GameEditor* GameEditor::instance = nullptr;
Object* EditorState::selectedObject = nullptr;
Scene* EditorState::activeScene = nullptr;
} // namespace Editor
