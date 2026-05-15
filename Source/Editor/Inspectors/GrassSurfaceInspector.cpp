#include "Editor/EditorState.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include <algorithm>

namespace Editor
{
class GrassSurfaceInspector : public Inspector<GrassSurface>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        GrassSurface* grassSurface = target;
        if (grassSurface == nullptr)
            return;

        if (ImGui::Button("Open Paint Tool"))
        {
            editor.OpenGrassSurfacePaintWindow(grassSurface);
        }

        auto& group = grassSurface->grassPatchGroup;

        EditorGUI::SeparatorTextLabeled("Grass Config");
        ImGui::ColorEdit3("Albedo", &group.config.albedo[0]);
        if (EditorGUI::DragFloat("Scale", &group.config.scale, 0.01f, 0.01f))
        {
            group.config.scale = glm::max(group.config.scale, 0.01f);
        }

        EditorGUI::SeparatorTextLabeled("Shadow Masks");
        for (int i = 0; i < 2; ++i)
        {
            ImGui::PushID(i);
            ObjPtr<Texture>& tex = (i == 0) ? group.config.grassShadowMask0 : group.config.grassShadowMask1;
            std::string label = tex.Get() ? tex->GetName() : "Slot " + std::to_string(i) + ": <empty>";
            ImGui::Text("%s", label.c_str());
            Texture* droppedTex = nullptr;
            if (EditorGUI::DropZone<Texture>("Drop Texture", droppedTex))
            {
                tex = droppedTex;
            }
            ImGui::PopID();
        }
        ImGui::DragFloat2("Mask 0 UV Scaler (XY)", &group.config.grassMaskUVScaler[0], 0.01f, 0.01f, 100.0f, "%.2f");
        ImGui::DragFloat2("Mask 1 UV Scaler (ZW)", &group.config.grassMaskUVScaler[2], 0.01f, 0.01f, 100.0f, "%.2f");

        EditorGUI::SeparatorTextLabeled("Color Ramp");
        ImGui::ColorEdit4("Ramp 1 Bottom", &group.config.grassColorRamp_Bottom[0]);
        ImGui::ColorEdit4("Ramp 1 Top", &group.config.grassColorRamp_Top[0]);
        ImGui::ColorEdit4("Ramp 2 Bottom", &group.config.grassColorRamp2_Bottom[0]);
        ImGui::ColorEdit4("Ramp 2 Top", &group.config.grassColorRamp2_Top[0]);
        ImGui::ColorEdit4("Ramp 3 Bottom", &group.config.grassColorRamp3_Bottom[0]);
        ImGui::ColorEdit4("Ramp 3 Top", &group.config.grassColorRamp3_Top[0]);
        ImGui::DragFloat("Hue Shift 0", &group.config.hueShift_0, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Hue Shift 1", &group.config.hueShift_1, 0.01f, -1.0f, 1.0f, "%.2f");

        ImGui::Text("Patches: %d", static_cast<int>(group.patches.size()));

        EditorGUI::SeparatorTextLabeled("Patch Meshes");
        for (int i = 0; i < static_cast<int>(group.patchMeshes.size()); ++i)
        {
            ImGui::PushID(i);

            Mesh* mesh = group.patchMeshes[i].Get();
            std::string label = mesh ? std::to_string(i) + ": " + mesh->GetName() : std::to_string(i) + ": <null>";
            if (ImGui::Button(label.c_str()))
            {
                EditorState::SelectObject(mesh);
            }

            Object* meshPayload = nullptr;
            if (EditorGUI::DragDropTarget(typeid(Mesh), meshPayload))
            {
                group.patchMeshes[i] = static_cast<Mesh*>(meshPayload);
            }

            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                group.patches.erase(
                    std::remove_if(
                        group.patches.begin(), group.patches.end(),
                        [i](const GrassPatch& patch)
                        {
                            return patch.meshIndex == i;
                        }
                    ),
                    group.patches.end()
                );

                group.patchMeshes.erase(group.patchMeshes.begin() + i);

                for (auto& patch : group.patches)
                {
                    if (patch.meshIndex > i)
                    {
                        patch.meshIndex--;
                    }
                }

                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        Mesh* meshPayload = nullptr;
        if (EditorGUI::DropZone<Mesh>("Drop Mesh Here", meshPayload))
        {
            group.patchMeshes.push_back(meshPayload);
        }
    }

private:
    static const char _register;
};

const char GrassSurfaceInspector::_register = InspectorRegistry::Register<GrassSurfaceInspector, GrassSurface>();
} // namespace Editor
