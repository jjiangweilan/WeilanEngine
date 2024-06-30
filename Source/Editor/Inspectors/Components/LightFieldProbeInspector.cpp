#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/LightFieldProbes.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/GameObject.hpp"
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
        Mesh* sphere = EngineInternalResources::GetModels().sphere;
        Shader* previewShader = GetLightFieldProbePreviewShader();

        glm::vec3 gridMin, gridMax;
        target->GetGrid(gridMin, gridMax);
        if (ImGui::SliderFloat3("gridMin", &gridMin[0], 0, 100) || ImGui::SliderFloat3("gridMax", &gridMax[0], 0, 100))
        {
            target->SetGrid(gridMin, gridMax);
        };

        glm::vec3 probeCount = target->GetProbeCounts();
        if (ImGui::InputFloat3("probeCount", &probeCount[0]))
        {
            target->SetProbeCounts(probeCount);
        };

        if (ImGui::Button("bake probes"))
        {
            target->BakeProbeCubemaps();
        }

        // draw gizmos
        glm::vec3 gridDelta = (gridMax - gridMin) / glm::max(probeCount - glm::vec3(1), glm::vec3(1, 1, 1));

        for (int x = 0; x < probeCount.x; ++x)
        {
            for (int y = 0; y < probeCount.y; ++y)
            {
                for (int z = 0; z < probeCount.z; ++z)
                {
                    glm::vec3 pos = target->GetGameObject()->GetPosition() + gridMin + glm::vec3(x, y, z) * gridDelta;
                    auto probe = target->GetProbe({x, y, z});
                    if (probe && probe->IsBaked())
                        Gizmos::DrawMesh(*sphere, 0, probe->GetPreviewMaterial(), glm::translate(glm::mat4(1.0f), pos));
                    else
                        Gizmos::DrawMesh(*sphere, 0, previewShader, glm::translate(glm::mat4(1.0f), pos));
                }
            }
        }
    }

private:
    Shader* GetLightFieldProbePreviewShader()
    {
        static Shader* previewShader;
        if (!previewShader)
        {
            previewShader = (Shader*)AssetDatabase::Singleton()->LoadAsset(
                "_engine_internal/Shaders/LightFieldProbes/LightFieldProbePreview.shad"
            );
        }
        return previewShader;
    }
};

static char _register = InspectorRegistry::Register<LightFieldProbesInspector, LightFieldProbes>();

} // namespace Editor
