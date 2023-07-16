#include "GameEditor.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"

namespace Engine::Editor
{

GameEditor::GameEditor(Scene& scene) : scene(scene)
{
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(GetGfxDriver()->GetSDLWindow());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
};

GameEditor::~GameEditor()
{
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void SceneTree(Transform* transform)
{
    if (ImGui::TreeNode(transform->GetGameObject()->GetName().c_str()))
    {
        GameObject* go = transform->GetGameObject();
        for (auto& c : go->GetComponents())
        {
            if (c->GetName() == "Transform")
            {
                auto pos = transform->GetPosition();
                if (ImGui::InputFloat3("Position", &pos[0]))
                {
                    transform->SetPosition(pos);
                }
            }
        }

        for (auto child : transform->GetChildren())
        {
            SceneTree(child.Get());
        }
        ImGui::TreePop();
    }
}

void GameEditor::Tick()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Scene");
    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root->GetTransform().Get());
    }
    ImGui::End();
    ImGui::ShowDemoWindow();
    ImGui::EndFrame();
}
} // namespace Engine::Editor
