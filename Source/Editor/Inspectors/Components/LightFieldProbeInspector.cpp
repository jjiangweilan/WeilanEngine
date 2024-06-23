#include ""
#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/LightFieldProbes.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/Gizmo.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Editor
{
class LightFieldProbesInspector : public Inspector<LightFieldProbes>
{
    void DrawInspector(GameEditor& editor) override
    {
        if (ImGui::Button("place probes"))
        {}

        Mesh* sphere = EngineInternalResources::GetModels().sphere;
        Shader* previewShader = GetLightFieldProbePreviewShader();

        glm::vec3 gridMin, gridMax;
        target->GetGrid(gridMin, gridMax);
        if (ImGui::InputFloat3("gridMin", &gridMin[0]) || ImGui::InputFloat3("gridMax", &gridMax[0]))
        {
            target->SetGrid(gridMin, gridMax);
        };

        auto probes = target->GetProbes();
        for (Rendering::LFP::Probe& probe : probes)
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), probe.GetPosition());
            Gizmos::DrawMesh(*sphere, 0, previewShader, m);
        }
    }

private:
};

static char _register = InspectorRegistry::Register<LightFieldProbesInspector, LightFieldProbes>();

} // namespace Editor
