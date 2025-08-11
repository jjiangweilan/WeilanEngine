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
        ImGui::Text("Projection Settings");
        ImGui::Indent();

        float nearPlane = target->GetNear();
        float farPlane = target->GetFar();
        float projTop = target->GetProjectionTop();
        float projRight = target->GetProjectionRight();
        uint32_t resolution = target->GetResolution();

        if (ImGui::DragFloat("Near: %.3f", &nearPlane))
            target->SetNear(nearPlane);
        if (ImGui::DragFloat("Far: %.3f", &farPlane))
            target->SetFar(farPlane);

        ImGui::Text("Projection Top: %.3f", projTop);
        ImGui::Text("Projection Right: %.3f", projRight);
        ImGui::Text("Resolution: %u", resolution);

        ImGui::Unindent();

        ImGui::Separator();

        // Cubemap display
        ImGui::Text("Generated Cubemap");
        if (Gfx::Image* cubemap = target->GetCubemap())
        {
            ImGui::Text("Cubemap: %p", cubemap);
            // TODO: Add cubemap preview when texture display is available
            // ImGui::Image(&cubemap->GetDefaultImageView(), {100, 100});
        }
        else
        {
            ImGui::Text("No cubemap generated");
        }

        ImGui::SeparatorText("Debug");

        // Debug information
        ImGui::Text("Debug Information");
        ImGui::Indent();
        ImGui::Text("Component Name: %s", target->GetName().c_str());
        ImGui::Text("Component UUID: %s", target->GetUUID().ToString().c_str());
        ImGui::Text("GameObject: %s", target->GetGameObject() ? target->GetGameObject()->GetName().c_str() : "None");
        ImGui::Unindent();

        // Actions
        ImGui::SeparatorText("Actions");
        if (ImGui::Button("Force Update"))
        {
            // TODO: Add method to force update the reflection probe
            // target->ForceUpdate();
        }

        ImGui::SameLine();
        if (ImGui::Button("Capture Probe"))
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
