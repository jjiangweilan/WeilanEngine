#include "Editor/EditorState.hpp"
#include "../Inspector.hpp"
#include "Engine/Runtime/Object/Component/SceneEnvironment.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Editor/EditorGUI.hpp"
#include "Engine/Library/ColorSpace.hpp"

namespace Editor
{
class SceneEnvironmentInspector : public Inspector<SceneEnvironment>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        auto sceneEnvironment = target;
        if (sceneEnvironment == nullptr)
            return;
        if (ImGui::CollapsingHeader("Skybox Probe"))
        {
            if (ImGui::Button("Update"))
            {
                sceneEnvironment->UpdateSkyboxProbe();
            }
        }

        if (ImGui::CollapsingHeader("Fog"))
        {
            auto& fog = sceneEnvironment->data.fogPassParameters;
            EditorGUI::Checkbox("Enabled", &fog.enabled);
            glm::vec4 displayFogColor = ColorSpace::LinearToSRGB(fog.fogColor);
            if (ImGui::ColorEdit4("Fog Color", &displayFogColor[0]))
                fog.fogColor = ColorSpace::SRGBToLinear(displayFogColor);
            EditorGUI::DragFloat("Fog Density", &fog.fogDensity, 0.01f, 0.0f);
        }
    }

private:
    bool debugSkyboxProbe = false;

    static const char _register;
};

const char SceneEnvironmentInspector::_register =
    InspectorRegistry::Register<SceneEnvironmentInspector, SceneEnvironment>();

} // namespace Editor
