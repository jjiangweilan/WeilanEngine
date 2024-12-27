#include "../EditorState.hpp"
#include "../GameEditor.hpp"
#include "Core/Scene/Scene.hpp"
#include "Inspector.hpp"

namespace Editor
{
class SceneInspector : public Inspector<Scene>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        // object information
        auto& name = target->GetName();
        char cname[1024];
        strcpy(cname, name.data());
        if (ImGui::InputText("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        if (ImGui::Button("Set as active scene"))
        {
            editor.SetActiveScene(target);
        }
    }

private:
    static const char _register;
};

const char SceneInspector::_register = InspectorRegistry::Register<SceneInspector, Scene>();

} // namespace Editor
