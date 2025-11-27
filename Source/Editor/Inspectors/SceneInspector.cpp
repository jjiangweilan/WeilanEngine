#include "Editor/EditorState.hpp"
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
        if (EditorGUI::InputTextLabeled("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        if (EditorGUI::ButtonSimple("Set as active scene"))
        {
            editor.SetActiveScene(target);
        }

        if (EditorGUI::ButtonSimple("Fix Undestroied GameObject Not In Scene Tree"))
        {
            target->FixUndestroiedGameObjectNotInSceneTree();
        }

        auto renderPipelineSetting = target->GetRenderPipelineSetting().Get();
        if (EditorGUI::ObjectField("Render Pipeline", renderPipelineSetting))
        {
            target->SetRenderPipelineSetting(renderPipelineSetting);
        }
    }

private:
    static const char _register;
};

const char SceneInspector::_register = InspectorRegistry::Register<SceneInspector, Scene>();

} // namespace Editor
