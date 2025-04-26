#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/Graphics.hpp"

namespace Editor
{
class LightInspector : public Inspector<Light>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Light* light = target;
        glm::vec3 lightColor = light->GetLightColor();
        float intensity = light->GetIntensity();
        float ambientScale = light->GetAmbientScale();
        int lightType = static_cast<int>(light->GetLightType());
        float shadowDistance = light->GetShadowDistance();
        const char* lightTypes[] = {"Directional", "Point"};

        ImGui::Combo("light type", &lightType, lightTypes, IM_ARRAYSIZE(lightTypes));
        if (light->GetLightType() != static_cast<LightType>(lightType))
        {
            light->SetLightType(static_cast<LightType>(lightType));
        }

        if (ImGui::InputFloat("Shadow Distance", &shadowDistance))
        {
            light->SetShadowDistance(shadowDistance);
        }

        if (ImGui::DragFloat("ambient scale", &ambientScale))
        {
            light->SetAmbientScale(ambientScale);
        }
        if (ImGui::ColorPicker3("lightColor", &lightColor[0]))
        {
            light->SetLightColor(lightColor);
        }
        if (ImGui::DragFloat("intensity", &intensity))
        {
            light->SetIntensity(intensity);
        }

        if (light->GetLightType() == LightType::Point)
        {
            float v1 = light->GetPointLightDistance();
            if (ImGui::DragFloat("point light distance", &v1))
            {
                light->SetPointLightDistance(v1);
            }
        }

        ImGui::Text("Shadow Cache");
        ImGui::SameLine();
        ImGui::Separator();
        if (!light->IsShadowCacheEnabled())
        {
            if (ImGui::Button("Enable Shadow Cache"))
            {
                light->EnableShadowCache();
            }
        }
        else
        {
            if (ImGui::Button("Disable Shadow Cache"))
            {
                light->DisableShadowCache();
            }
        }

        int targetFrames = light->GetShadowCacheTargetFrames();
        if (ImGui::InputInt("target frames", &targetFrames))
        {
            light->SetShadowUpdateFrames(targetFrames);
        }
        ImGui::Separator();

        Frustum frustum = target->GetLightFrusutmPlanes(target->GetGameObject()->GetPosition());

        // 6 different float4 colors
        float4 colors[6] = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}, {0, 1, 1, 1}, {1, 0, 1, 1}};

        auto cam = target->GetScene()->GetMainCamera();
        for (int i = 0; i < 6; ++i)
        {
            Graphics::DrawLine(
                cam->GetGameObject()->GetPosition(),
                -float3(frustum.planes[i]) * frustum.planes[i].a + cam->GetGameObject()->GetPosition(),
                colors[i]
            );
        }
    }

private:
    static const char _register;
};

const char LightInspector::_register = InspectorRegistry::Register<LightInspector, Light>();

} // namespace Editor
