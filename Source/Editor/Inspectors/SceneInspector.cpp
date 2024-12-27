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

        if (ImGui::Button("Fix Undestroied GameObject Not In Scene Tree"))
        {
            target->FixUndestroiedGameObjectNotInSceneTree();
        }
    }

private:
    static const char _register;
};

const char SceneInspector::_register = InspectorRegistry::Register<SceneInspector, Scene>();

} // namespace Editor
