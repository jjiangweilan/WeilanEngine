#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/Light.hpp"

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
        const char* lightTypes[] = {"Directional", "Point"};

        ImGui::Combo("light type", &lightType, lightTypes, IM_ARRAYSIZE(lightTypes));
        if (light->GetLightType() != static_cast<LightType>(lightType))
        {
            light->SetLightType(static_cast<LightType>(lightType));
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
            float v0 = light->GetPointLightLinear();
            if (ImGui::DragFloat("point light intensity 1", &v0))
            {
                light->SetPointLightLinear(v0);
            }

            float v1 = light->GetPointLightQuadratic();
            if (ImGui::DragFloat("point light intensity 2", &v1))
            {
                light->SetPointLightQuadratic(v1);
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
    }

private:
    static const char _register;
};

const char LightInspector::_register = InspectorRegistry::Register<LightInspector, Light>();

} // namespace Editor
