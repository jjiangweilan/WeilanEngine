#include "NavDataBakeWindow.hpp"

#include "Editor/EditorGUI.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"
#include "Engine/Runtime/System/Navigation/NavDataBaker.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{
DEFINE_EDITOR_WINDOW(NavDataBakeWindow, "Tools/Nav Data Baker")

bool NavDataBakeWindow::Tick()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Nav Data Baker", &open))
    {
        Mesh* meshPtr = mesh.Get();
        if (EditorGUI::ObjectField("Mesh", meshPtr))
        {
            mesh = meshPtr;
        }

        NavData* navDataPtr = navData.Get();
        if (EditorGUI::ObjectField("Nav Data", navDataPtr))
        {
            navData = navDataPtr;
        }

        ImGui::Separator();
        if (navDataPtr != nullptr)
        {
            const NavDataConfig& config = navDataPtr->grid.config;
            ImGui::Text("Resolution: %.3f", config.resolution);
            ImGui::Text("Grid: %d x %d", config.width, config.height);
            ImGui::Text("Cells: %zu", navDataPtr->grid.cells.size());
        }
        else
        {
            ImGui::TextDisabled("Assign a NavData asset to bake into.");
        }

        const bool canBake = meshPtr != nullptr && navDataPtr != nullptr;
        if (!canBake)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Bake"))
        {
            NavDataBaker baker;
            baker.Bake(meshPtr, *navDataPtr);
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
