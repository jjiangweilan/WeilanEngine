#include "SceneTreeWindow.hpp"
#include "Core/Component/Components.hpp"
#include "../imgui/imgui.h"

#include <spdlog/spdlog.h>
namespace Engine::Editor
{
    SceneTreeWindow::SceneTreeWindow(RefPtr<EditorContext> editorContext) : editorContext(editorContext)
    {

    }

    void SceneTreeWindow::Tick()
    {
        static bool isOpen = false;
        ImGui::Begin("Scene Tree", &isOpen);
        auto gameScene =GameSceneManager::Instance()->GetActiveGameScene();
        if (gameScene)
        {
            auto& roots = gameScene->GetRootObjects();
            uint32_t id = 0;
            for(auto root : roots)
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
        }

        ImGui::End();

    }

    void SceneTreeWindow::DisplayGameObject(RefPtr<GameObject> obj, uint32_t& id)
    {
        std::string title = obj->GetName() + "##" + std::to_string(id++);
        if(ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_OpenOnArrow))
        {
            for(auto child : obj->GetTransform()->GetChildren())
            {
                DisplayGameObject(child->GetGameObject(), id);
            }
            ImGui::TreePop();
        }

        bool clickedOnArrow = (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x) < ImGui::GetTreeNodeToLabelSpacing();
        if (ImGui::IsItemClicked(0) && !clickedOnArrow)
        {
            editorContext->currentSelected = obj;
        }
    }
}
