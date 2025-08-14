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
        if (GUI::InputTextLabeled("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        if (GUI::ButtonSimple("Set as active scene"))
        {
            editor.SetActiveScene(target);
        }

        if (GUI::ButtonSimple("Fix Undestroied GameObject Not In Scene Tree"))
        {
            target->FixUndestroiedGameObjectNotInSceneTree();
        }

        auto renderPipelineSetting = target->GetRenderPipelineSetting().Get();
        if (GUI::ObjectField("Render Pipeline", renderPipelineSetting))
        {
            target->SetRenderPipelineSetting(renderPipelineSetting);
        }
    }

private:
    static const char _register;
};

const char SceneInspector::_register = InspectorRegistry::Register<SceneInspector, Scene>();

} // namespace Editor
