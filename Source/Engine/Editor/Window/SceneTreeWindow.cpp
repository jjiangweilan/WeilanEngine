#include "SceneTreeWindow.hpp"
#include "Core/Component/Components.hpp"
#include "ThirdParty/imgui/imgui.h"

#include <spdlog/spdlog.h>
#define DYNAMIC_CAST_PAYLOAD(data, Type) dynamic_cast<Type*>(*(AssetObject**)data)
namespace Engine::Editor
{
SceneTreeWindow::SceneTreeWindow(RefPtr<EditorContext> editorContext) : editorContext(editorContext) {}

void SceneTreeWindow::Tick()
{
    static bool isOpen = false;
    ImGui::Begin("Scene Tree", &isOpen);
    auto gameScene = GameSceneManager::Instance()->GetActiveGameScene();
    if (gameScene)
    {
        auto& roots = gameScene->GetRootObjects();
        uint32_t id = 0;
        for (auto root : roots)
        {
            DisplayGameObject(root, id);
        }

        // create game object
        if (ImGui::BeginPopupContextWindow() && !ImGui::IsItemHovered())
        {
            if (ImGui::Selectable("Create GameObject"))
            {
                gameScene->CreateGameObject();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::GetDragDropPayload();
            if (AssetObject* aobj = DYNAMIC_CAST_PAYLOAD(payload->Data, AssetObject))
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GameObject");
                if ((aobj->GetTypeInfo() == typeid(GameObject)) && payload)
                {
                    ImGui::EndDragDropTarget();
                    gameScene->AddGameObject((GameObject*)aobj);
                    return;
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
}

void SceneTreeWindow::DisplayGameObject(RefPtr<GameObject> obj, uint32_t& id)
{
    std::string title = obj->GetName() + "##" + std::to_string(id++);

    bool opened = ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
    bool clickedOnArrow = (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x) < ImGui::GetTreeNodeToLabelSpacing();
    if (ImGui::IsItemClicked(0) && !clickedOnArrow)
    {
        editorContext->currentSelected = obj;
    }
    if (opened)
    {
        for (auto child : obj->GetTransform()->GetChildren())
        {
            DisplayGameObject(child->GetGameObject(), id);
        }
        ImGui::TreePop();
    }
}
} // namespace Engine::Editor
