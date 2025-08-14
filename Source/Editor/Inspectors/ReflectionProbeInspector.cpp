#include "../EditorState.hpp"
#include "Core/Component/ReflectionProbe.hpp"
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
        if (GUI::EnumDropDown(
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
            target->SetUpdateType(static_cast<ReflectionProbe::UpdateType>(updateType));
        }

        ImGui::Separator();

        // Projection settings (read-only display)
        GUI::SeparatorTextLabeled("Projection Settings");

        float nearPlane = target->GetNear();
        float farPlane = target->GetFar();
        float projTop = target->GetProjectionTop();
        float projRight = target->GetProjectionRight();
        uint32_t resolution = target->GetResolution();

        if (GUI::DragFloat("Near", &nearPlane))
            target->SetNear(nearPlane);
        if (GUI::DragFloat("Far", &farPlane))
            target->SetFar(farPlane);

        GUI::TextFormatted("Projection Top", "%.3f", projTop);
        GUI::TextFormatted("Projection Right", "%.3f", projRight);
        GUI::TextFormatted("Resolution", "%u", resolution);

        GUI::ObjectPropertyEnum(
            "SourceType",
            {"Static", "Runtime"},
            *target,
            &ReflectionProbe::GetSourceType,
            &ReflectionProbe::SetSourceType
        );

        ImGui::Separator();

        // Cubemap display
        GUI::SeparatorTextLabeled("Generated Cubemap");
        if (Gfx::Image* cubemap = target->GetCubemap())
        {
            GUI::TextFormatted("Cubemap", "%p", cubemap);
            // TODO: Add cubemap preview when texture display is available
            // ImGui::Image(&cubemap->GetDefaultImageView(), {100, 100});
        }
        else
        {
            GUI::Text("Cubemap", "No cubemap generated");
        }

        GUI::SeparatorTextLabeled("Debug");

        // Debug information
        GUI::Text("Component Name", target->GetName().c_str());
        GUI::Text("Component UUID", target->GetUUID().ToString().c_str());
        GUI::Text("GameObject", target->GetGameObject() ? target->GetGameObject()->GetName().c_str() : "None");

        // Actions
        GUI::SeparatorTextLabeled("Actions");
        if (GUI::ButtonSimple("Force Update"))
        {
            // TODO: Add method to force update the reflection probe
            // target->ForceUpdate();
        }

        ImGui::SameLine();
        if (GUI::ButtonSimple("Capture Probe"))
        {
            // TODO: Add method to bake the reflection probe
            // target->Bake();
        }

        for (int i = 0; i < 6; ++i)
        {
            Graphics::DrawFrustum(target->GetProjectionMatrix() * target->GetViewMatrix(i));
        }
    }

private:
    static const char _register;
};

const char ReflectionProbeInspector::_register =
    InspectorRegistry::Register<ReflectionProbeInspector, ReflectionProbe>();

} // namespace Editor
