#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/Texture.hpp"

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
    }

private:
    bool debugSkyboxProbe = false;


    static const char _register;
};

const char SceneEnvironmentInspector::_register =
    InspectorRegistry::Register<SceneEnvironmentInspector, SceneEnvironment>();

} // namespace Editor
