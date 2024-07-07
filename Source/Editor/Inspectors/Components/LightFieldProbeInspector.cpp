#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/LightFieldProbes.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/GameObject.hpp"
#include "Core/Gizmo.hpp"
#include "Rendering/GlobalIllumination/LightFieldProbes/ProbeBaker.hpp"
#include "Rendering/Graphics.hpp"
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

        if (ImGui::Checkbox("debug bake", &debug))
        {}

        if (ImGui::Button("bake probes"))
        {
            previewMaterials.clear();
            target->BakeProbeCubemaps(debug);
            for (auto& probe : target->GetProbes())
            {
                auto material = std::make_unique<Material>(GetLightFieldProbePreviewShader());
                material->SetTexture("albedoTex", probe.GetAlbedo());
                material->SetTexture("normalTex", probe.GetNormal());
                material->SetTexture("radialDistanceTex", probe.GetRadialDistance());
                material->EnableFeature("Baked");
                previewMaterials.push_back(std::move(material));
            }
        }

        static bool showCubemap;
        if (ImGui::Button("Show cubemap"))
        {
            showCubemap = !showCubemap;
        }

        if (ImGui::Button("Show cubemap VP"))
        {
            showCubemapFrustum = !showCubemapFrustum;
        }

        // draw gizmos
        glm::vec3 gridDelta = (gridMax - gridMin) / glm::max(probeCount - glm::vec3(1), glm::vec3(1, 1, 1));

        for (int z = 0; z < probeCount.z; ++z)
        {
            for (int y = 0; y < probeCount.y; ++y)
            {
                for (int x = 0; x < probeCount.x; ++x)
                {
                    glm::vec3 pos = target->GetGameObject()->GetPosition() + gridMin + glm::vec3(x, y, z) * gridDelta;
                    auto probe = target->GetProbe({x, y, z});
                    auto probeIndex = target->GetProbeIndex({x, y, z});
                    if (probe && probe->IsBaked())
                    {
                        if (showCubemapFrustum)
                        {
                            auto baker = target->GetProbeBaker({x, y, z});
                            if (baker)
                            {
                                for (auto& face : baker->GetFaces())
                                {
                                    Graphics::DrawFrustum(face.vp);
                                }
                            }
                        }

                        if (probeIndex < previewMaterials.size())
                        {
                            auto& mat = previewMaterials[probeIndex];
                            if (!showCubemap)
                            {
                                mat->EnableFeature("Baked");
                                mat->DisableFeature("Cubemap");
                            }
                            else
                            {
                                mat->DisableFeature("Baked");
                                mat->EnableFeature("Cubemap");
                            }
                            Gizmos::DrawMesh(*sphere, 0, mat.get(), glm::translate(glm::mat4(1.0f), pos));
                        }
                        else
                        {
                            Gizmos::DrawMesh(*sphere, 0, previewShader, glm::translate(glm::mat4(1.0f), pos));
                        }
                    }
                    else
                        Gizmos::DrawMesh(*sphere, 0, previewShader, glm::translate(glm::mat4(1.0f), pos));
                }
            }
        }
    }

private:
    bool showCubemapFrustum = false;
    bool debug = false;
    std::vector<std::unique_ptr<Material>> previewMaterials;
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
