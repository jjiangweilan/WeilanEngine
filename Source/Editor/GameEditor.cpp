#include "GameEditor.hpp"
#include "Core/Asset.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Time.hpp"
#include "EditorState.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Inspectors/Inspector.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include "Tools/EnvironmentBaker.hpp"
#include "WeilanEngine.hpp"
#include "spdlog/spdlog.h"
#include <unordered_map>

namespace Engine::Editor
{
GameEditor::GameEditor(const char* path)
{
    engine = std::make_unique<WeilanEngine>();
    engine->Init({.projectPath = path});
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

    uint32_t mainQueueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();
};

GameEditor::~GameEditor()
{
    engine->gfxDriver->WaitForIdle();
}
void GameEditor::SceneTree(Transform* transform, int imguiID)
{
    ImGuiTreeNodeFlags nodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (transform->GetGameObject() == EditorState::selectedObject)
        nodeFlags |= ImGuiTreeNodeFlags_Selected;

    bool treeOpen =
        ImGui::TreeNodeEx(fmt::format("{}##{}", transform->GetGameObject()->GetName(), imguiID).c_str(), nodeFlags);
    if (ImGui::IsItemClicked())
    {
        EditorState::selectedObject = transform->GetGameObject();
    }

    if (treeOpen)
    {
        GameObject* go = transform->GetGameObject();
        for (auto child : transform->GetChildren())
        {
            SceneTree(child, imguiID++);
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

    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root->GetTransform(), 0);
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
        int index = 0;
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

    if (ImGui::MenuItem("Editor Camera"))
    {
        if (EditorState::activeScene)
        {
            gameView.gameCamera = EditorState::activeScene->GetMainCamera();
            EditorState::activeScene->SetMainCamera(gameView.editorCamera);
        }
    }
    if (ImGui::MenuItem("Game Camera"))
    {
        if (EditorState::activeScene)
        {
            EditorState::activeScene->SetMainCamera(gameView.gameCamera);
            gameView.gameCamera = nullptr;
        }
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

bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj)
{
    auto mr = obj->GetComponent<MeshRenderer>();
    if (mr)
    {
        for (const Submesh& submesh : mr->GetMesh()->GetSubmeshes())
        {
            uint8_t* vertexBufferData = submesh.GetVertexBufferData();
            auto bindings = submesh.GetBindings();
            if (bindings.empty() || vertexBufferData == nullptr)
                continue;

            auto positionBinding = bindings[0];

            // I just assume binding zero is a vec3 position, this is not robust
            for (int i; i < submesh.GetIndexCount(); i += 3)
            {
                int j = i + 1;
                int k = i + 2;

                glm::vec3 v0, v1, v2;
                glm::vec3* positionData = reinterpret_cast<glm::vec3*>(vertexBufferData + positionBinding.byteOffset);
                memcpy(&v0, positionData + i, 12);
                memcpy(&v1, positionData + j, 12);
                memcpy(&v2, positionData + k, 12);

                glm::vec2 bary;
                float distance;
                return glm::intersectRayTriangle(ori, dir, v0, v1, v2, bary, distance);
            }
        }
    }

    // ray is not tested, there is no mesh maybe use a box as proxy?
    return false;
}

void GameEditor::PickGameObjectFromScene()
{
    if (Scene* scene = EditorState::activeScene)
    {
        auto objs = scene->GetAllGameObjects();
        auto mainCam = scene->GetMainCamera();
        auto camTsm = mainCam->GetGameObject()->GetTransform();
        auto forward = camTsm->GetForward();
        auto pos = camTsm->GetPosition();

        std::vector<GameObject*> intersected;
        for (auto obj : objs)
        {}
    }
}

void GameEditor::GUIPass()
{
    MainMenuBar();
    OpenSceneWindow();

    gameView.Tick();
    AssetWindow();
    InspectorWindow();

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

                inspector->DrawInspector();
            }
        }

        ImGui::End();

        if (lockWindow)
        {
            ImGui::Begin("Secondary Inspector", &lockWindow, ImGuiWindowFlags_MenuBar);

            if (EditorState::selectedObject)
            {
                InspectorBase* inspector = InspectorRegistry::GetInspector(*EditorState::selectedObject);
                inspector->DrawInspector();
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
    }
}

Object* EditorState::selectedObject = nullptr;
Scene* EditorState::activeScene = nullptr;
} // namespace Engine::Editor
