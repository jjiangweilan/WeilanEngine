#include "GameEditor.hpp"
#include "Core/Asset.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Time.hpp"
#include "EditorState.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Inspectors/Inspector.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "Tools/EnvironmentBaker.hpp"
#include "spdlog/spdlog.h"
#include <unordered_map>

namespace Engine::Editor
{
GameEditor::GameEditor(const char* path)
{
    instance = this;
    engine = std::make_unique<WeilanEngine>();
    engine->Init({.projectPath = path});

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

    gameView.Init();

    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    gameEditorRenderer = std::make_unique<Editor::Renderer>();

    auto [editorRenderNode, editorRenderNodeOutputHandle] = gameEditorRenderer->BuildGraph();
    gameEditorRenderer->Process(editorRenderNode, editorRenderNodeOutputHandle);

    toolList.emplace_back(new EnvironmentBaker());

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
    if (EditorState::activeScene)
        editorConfig["lastActiveScene"] = EditorState::activeScene->GetUUID().ToString();

    if (Camera* cam = gameView.GetEditorCamera())
    {
        nlohmann::json camJson = {};
        auto pos = cam->GetGameObject()->GetTransform()->GetPosition();
        auto rot = cam->GetGameObject()->GetTransform()->GetRotationQuat();
        auto scale = cam->GetGameObject()->GetTransform()->GetScale();
        camJson["position"] = {pos.x, pos.y, pos.z};
        camJson["rotation"] = {rot.w, rot.x, rot.y, rot.z};
        camJson["scale"] = {scale.x, scale.y, scale.z};
        editorConfig["editorCamera"] = camJson;
    }

    auto editorConfigPath = engine->GetProjectPath() / "editorConfig.json";
    std::ofstream editorConfigFile(editorConfigPath);
    editorConfigFile << editorConfig.dump(0);
}

void GameEditor::SceneTree(Transform* transform, int imguiID)
{
    ImGuiTreeNodeFlags nodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (transform->GetGameObject() == EditorState::selectedObject)
        nodeFlags |= ImGuiTreeNodeFlags_Selected;

    bool treeOpen =
        ImGui::TreeNodeEx(fmt::format("{}##{}", transform->GetGameObject()->GetName(), imguiID).c_str(), nodeFlags);
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        EditorState::selectedObject = transform->GetGameObject();
    }

    if (ImGui::BeginDragDropSource())
    {
        auto ptr = transform->GetGameObject();
        ImGui::SetDragDropPayload("game object", &ptr, sizeof(void*));

        ImGui::Text("%s", transform->GetGameObject()->GetName().c_str());

        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("game object");
        if (payload && payload->IsDelivery())
        {
            GameObject* obj = *(GameObject**)payload->Data;
            if (obj != nullptr && obj != transform->GetGameObject())
            {
                obj->GetTransform()->SetParent(transform);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (treeOpen)
    {
        for (auto child : transform->GetChildren())
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
                obj->GetTransform()->SetParent(nullptr);
            }
        }
        ImGui::EndDragDropTarget();
    }

    size_t imguiTreeId = 0;
    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root->GetTransform(), ++imguiTreeId);
    }
    ImGui::End();
}

void MenuVisitor(std::vector<std::string>::iterator iter, std::vector<std::string>::iterator end, bool& clicked)
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

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void GameEditor::Start()
{
    while (true)
    {
        engine->BeginFrame();

        if (engine->event->GetWindowClose().state)
        {
            return;
        }
        if (engine->event->GetWindowSizeChanged().state)
        {
            auto [editorRenderNode, editorRenderNodeOutputHandle] = gameEditorRenderer->BuildGraph();
            gameEditorRenderer->Process(editorRenderNode, editorRenderNodeOutputHandle);
        }

        GUIPass();

        auto& cmd = engine->GetActiveCmdBuffer();
        Render(cmd);

        engine->EndFrame();
    }
}

void GameEditor::GUIPass()
{
    MainMenuBar();
    OpenSceneWindow();

    gameView.Tick();
    AssetWindow();
    InspectorWindow();

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_R))
    {
        engine->assetDatabase->RequestShaderRefresh(true);
    }

    if (EditorState::activeScene)
    {
        if (EditorState::activeScene)
            SceneTree(*EditorState::activeScene);
    }
}

void GameEditor::Render(Gfx::CommandBuffer& cmd)
{
    // make sure we don't have sync issue with game rendering
    Gfx::GPUBarrier barrier{
        .buffer = nullptr,
        .image = nullptr,
        .srcStageMask = Gfx::PipelineStage::All_Commands,
        .dstStageMask = Gfx::PipelineStage::All_Commands,
        .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
    };
    cmd.Barrier(&barrier, 1);

    for (auto& t : registeredTools)
    {
        if (t.isOpen)
            t.tool->Render(cmd);
    }

    if (EditorState::activeScene)
        gameView.Render(cmd, EditorState::activeScene);

    // make sure we don't have  sync issue with tool rendering
    cmd.Barrier(&barrier, 1);

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
                InspectorBase* inspector = InspectorRegistry::GetInspector(*primarySelected);

                inspector->DrawInspector(*this);
            }
        }

        ImGui::End();

        if (lockWindow)
        {
            ImGui::Begin("Secondary Inspector", &lockWindow, ImGuiWindowFlags_MenuBar);

            if (EditorState::selectedObject)
            {
                InspectorBase* inspector = InspectorRegistry::GetInspector(*EditorState::selectedObject);
                inspector->DrawInspector(*this);
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

GameEditor* GameEditor::instance = nullptr;
Object* EditorState::selectedObject = nullptr;
Scene* EditorState::activeScene = nullptr;
} // namespace Engine::Editor
