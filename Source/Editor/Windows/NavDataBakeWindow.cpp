#include "NavDataBakeWindow.hpp"

#include "Editor/EditorGUI.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"
#include "Engine/Runtime/System/Navigation/NavDataBaker.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

#include <vector>

namespace Editor
{
DEFINE_EDITOR_WINDOW(NavDataBakeWindow, "Tools/Nav Data Baker")

bool NavDataBakeWindow::Tick()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Nav Data Baker", &open))
    {
        GameObject* rootObjectPtr = rootObject.Get();
        if (EditorGUI::ObjectField("Root Object", rootObjectPtr))
        {
            rootObject = rootObjectPtr;
        }

        NavData* navDataPtr = navData.Get();
        if (EditorGUI::ObjectField("Nav Data", navDataPtr))
        {
            navData = navDataPtr;
        }

        ImGui::Separator();
        std::vector<MeshRenderer*> renderers;
        if (rootObjectPtr != nullptr)
            renderers = rootObjectPtr->GetComponentsInChildren<MeshRenderer>();

        ImGui::Text("Mesh Renderers: %zu", renderers.size());
        if (navDataPtr != nullptr)
        {
            const NavDataConfig& config = navDataPtr->grid.config;
            ImGui::Text("Resolution: %.3f x %.3f", config.resolution.x, config.resolution.y);
            ImGui::Text("Grid: %d x %d", config.width, config.height);
            ImGui::Text("Origin: %.3f, %.3f, %.3f",
                config.origin.x, config.origin.y, config.origin.z);
            ImGui::Text("Cells: %zu", navDataPtr->grid.cells.size());
        }
        else
        {
            ImGui::TextDisabled("Assign a NavData asset to bake into.");
        }

        const bool canBake = !renderers.empty() && navDataPtr != nullptr;
        if (!canBake)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Bake"))
        {
            NavDataBaker baker;
            baker.Bake(renderers, *navDataPtr);
            navDataPtr->SetDirty();
        }

        if (!canBake)
        {
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
    return open;
}
} // namespace Editor
