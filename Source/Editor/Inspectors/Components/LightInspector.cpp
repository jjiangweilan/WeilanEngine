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
    }

private:
    static const char _register;
};

const char LightInspector::_register = InspectorRegistry::Register<LightInspector, Light>();

} // namespace Editor
