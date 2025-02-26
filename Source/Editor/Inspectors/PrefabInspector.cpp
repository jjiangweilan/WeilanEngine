#include "Core/Prefab.hpp"
#include "EditorGUI.hpp"
#include "GameObjectInspector.hpp"
#include "Inspector.hpp"

namespace Editor
{
class PrefabInspector : public Inspector<Prefab>
{
public:
    void OnEnable(Object& obj) override
    {
        Inspector<Prefab>::OnEnable(obj);

        goTarget = target->GetGameObject();
        if (goTarget)
            gameObjectInspector.OnEnable(*goTarget);
    }
    void DrawInspector(GameEditor& editor) override
    {
        ImGui::SeparatorText("GameObject");

        if (goTarget)
        {
            gameObjectInspector.DrawInspector(editor);
        }
    }

private:
    static const char _register;
    GameObject* goTarget = nullptr;
    GameObjectInspector gameObjectInspector;
};

const char PrefabInspector::_register = InspectorRegistry::Register<PrefabInspector, Prefab>();

} // namespace Editor
