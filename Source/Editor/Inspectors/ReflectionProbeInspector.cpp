#include "../EditorState.hpp"
#include "Core/Component/ReflectionProbe.hpp"
#include "Editor/Gizmos/ScaleBoxGizmo.hpp"
#include "EditorGUI.hpp"
#include "GameEditor.hpp"
#include "Inspector.hpp"
#include "Rendering/Graphics.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "WeilanEngine.hpp"

namespace Editor
{
class ReflectionProbeInspector : public Inspector<ReflectionProbe>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        // Base inspector functionality (name, UUID, etc.)
        Inspector<ReflectionProbe>::DrawInspector(editor);

        ImGui::Separator();

        // Update Type
        int updateType = static_cast<int>(target->GetUpdateType());
        if (EditorGUI::EnumDropDown(
                "Update Type",
                updateType,
                1,
                [](int val) -> std::string
                {
                    switch (val)
                    {
                        case 0: return "Local";
                        default: return "Unknown";
                    }
                }
            ))
        {
            target->SetUpdateType(static_cast<ReflectionProbe::ProbeType>(updateType));
        }

        ImGui::Separator();

        // Projection settings (read-only display)
        EditorGUI::SeparatorTextLabeled("Projection Settings");

        float nearPlane = target->GetNear();
        float farPlane = target->GetFar();
        float projTop = target->GetProjectionTop();
        float projRight = target->GetProjectionRight();
        uint32_t resolution = target->GetResolution();

        if (EditorGUI::DragFloat("Near", &nearPlane))
            target->SetNear(nearPlane);
        if (EditorGUI::DragFloat("Far", &farPlane))
            target->SetFar(farPlane);

        EditorGUI::TextFormatted("Projection Top", "%.3f", projTop);
        EditorGUI::TextFormatted("Projection Right", "%.3f", projRight);
        EditorGUI::TextFormatted("Resolution", "%u", resolution);

        EditorGUI::ObjectPropertyEnum(
            "SourceType",
            {"Static", "Runtime"},
            *target,
            &ReflectionProbe::GetSourceType,
            &ReflectionProbe::SetSourceType
        );

        ImGui::Separator();

        // Cubemap display
        EditorGUI::SeparatorTextLabeled("Generated Cubemap");
        if (Gfx::Image* cubemap = target->GetCubemap())
        {
            EditorGUI::TextFormatted("Cubemap", "%p", cubemap);
            // TODO: Add cubemap preview when texture display is available
            // ImGui::Image(&cubemap->GetDefaultImageView(), {100, 100});
        }
        else
        {
            EditorGUI::Text("Cubemap", "No cubemap generated");
        }

        EditorGUI::SeparatorTextLabeled("Debug");

        // Debug information
        EditorGUI::Text("Component Name", target->GetName().c_str());
        EditorGUI::Text("Component UUID", target->GetUUID().ToString().c_str());
        EditorGUI::Text("GameObject", target->GetGameObject() ? target->GetGameObject()->GetName().c_str() : "None");

        // Actions
        EditorGUI::SeparatorTextLabeled("Actions");
        if (EditorGUI::ButtonSimple("Force Update"))
        {
            // TODO: Add method to force update the reflection probe
            // target->ForceUpdate();
        }

        ImGui::SameLine();
        if (EditorGUI::ButtonSimple("Capture Probe"))
        {
            // TODO: Add method to bake the reflection probe
            // target->Bake();
        }

        for (int i = 0; i < 6; ++i)
        {
            Graphics::DrawFrustum(target->GetProjectionMatrix() * target->GetViewMatrix(i));
        }

        auto extent = target->GetExtent();
        auto position = target->GetGameObject()->GetPosition();
        editor.GetEditorContext()->GetGizmoManager()->Draw<ScaleBoxGizmo>(
            scaleBoxGizmoHandle,
            position,
            target->GetGameObject()->GetRotation(),
            extent
        );
        target->SetExtent(extent);
        target->GetGameObject()->SetPosition(position);
    }

private:
    static const char _register;
    GizmoHandle scaleBoxGizmoHandle{};
};

const char ReflectionProbeInspector::_register =
    InspectorRegistry::Register<ReflectionProbeInspector, ReflectionProbe>();

} // namespace Editor
