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
        float intensity = light->GetIntensity();
        ImGui::DragFloat("intensity", &intensity);
        light->SetIntensity(intensity);
    }

private:
    static const char _register;
};

const char LightInspector::_register = InspectorRegistry::Register<LightInspector, Light>();

} // namespace Editor
