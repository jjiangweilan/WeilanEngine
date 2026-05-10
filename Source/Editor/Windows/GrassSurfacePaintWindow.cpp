#include "GrassSurfacePaintWindow.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/SceneEditor.hpp"
#include "Editor/SceneEditorTool.hpp"
#include "Editor/Tools/GrassSurfacePaintTool.hpp"
#include "Editor/EditorGUI.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{

DEFINE_EDITOR_WINDOW(GrassSurfacePaintWindow, "Tools/Grass Paint")

bool GrassSurfacePaintWindow::Tick()
{
    bool open = true;
    if (ImGui::Begin("Grass Paint", &open))
    {
        if (!tool)
        {
            tool = std::make_unique<GrassSurfacePaintTool>();
        }

        ImGui::Text("Status: %s", isActive ? "Active" : "Inactive");
        ImGui::Text("Target: %s", tool->GetTargetName().c_str());
        ImGui::Text("Patches: %d", tool->GetPatchCount());
        ImGui::Separator();

        ImGui::SliderFloat("Brush Radius", &tool->brushRadius, 0.1f, 10.0f, "%.2f");
        ImGui::SliderInt("Density", &tool->density, 1, 20);
        ImGui::Checkbox("Erase Mode", &tool->eraseMode);

        ImGui::Separator();

        // Patch Meshes Section
        GrassSurface* gs = tool->GetTargetGrassSurface();
        if (gs)
        {
            ImGui::Text("Patch Meshes");
            auto& patchMeshes = gs->grassPatchGroup.patchMeshes;
            auto& patches = gs->grassPatchGroup.patches;

            for (int i = 0; i < (int)patchMeshes.size(); ++i)
            {
                ImGui::PushID(i);

                bool isSelected = (tool->meshIndex == i);
                std::string name = patchMeshes[i] ? patchMeshes[i]->GetName() : "<null>";

                if (ImGui::RadioButton("##select", isSelected))
                {
                    tool->meshIndex = i;
                }
                ImGui::SameLine();
                ImGui::Text("[%d] %s", i, name.c_str());
                ImGui::SameLine();

                float removeButtonWidth = ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2;
                ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - removeButtonWidth);
                if (ImGui::Button("X"))
                {
                    // Delete all patches using this mesh index
                    patches.erase(
                        std::remove_if(
                            patches.begin(), patches.end(),
                            [&](const GrassPatch& p) { return p.meshIndex == i; }
                        ),
                        patches.end()
                    );

                    // Remove the mesh
                    patchMeshes.erase(patchMeshes.begin() + i);

                    // Decrement indices of patches that referenced higher meshes
                    for (auto& p : patches)
                    {
                        if (p.meshIndex > i)
                            p.meshIndex--;
                    }

                    // Clamp meshIndex
                    tool->ClampMeshIndex();
                }

                ImGui::PopID();
            }

            // Drop zone
            ImGui::Button("Drop Mesh Here", ImVec2(-1, 30));
            Object* meshPayload = nullptr;
            if (EditorGUI::DragDropTarget(typeid(Mesh), meshPayload))
            {
                Mesh* mesh = static_cast<Mesh*>(meshPayload);
                if (mesh)
                {
                    patchMeshes.push_back(mesh);
                    tool->meshIndex = (int)patchMeshes.size() - 1;
                }
            }
        }
        else
        {
            ImGui::TextDisabled("Hover over a GrassSurface to manage patch meshes.");
        }

        ImGui::Separator();
        if (isActive)
        {
            if (ImGui::Button("Deactivate"))
            {
                if (GameEditor::instance)
                    GameEditor::instance->SetActiveSceneEditorTool(nullptr);
                isActive = false;
            }
        }
        else
        {
            if (ImGui::Button("Activate"))
            {
                if (GameEditor::instance)
                    GameEditor::instance->SetActiveSceneEditorTool(tool.get());
                isActive = true;
            }
        }
    }
    ImGui::End();
    return open;
}

} // namespace Editor
