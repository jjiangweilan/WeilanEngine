#include "Editor/EditorGUI.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "GameObjectInspector.hpp"

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
            JsonSerializer s;
            goTarget->Serialize(&s);

            gameObjectInspector.DrawInspector(editor);

            JsonSerializer ss;
            goTarget->Serialize(&ss);

            if (s.GetJson() != ss.GetJson())
            {
                target->SetDirty(true);
            }
        }
    }

private:
    static const char _register;
    GameObject* goTarget = nullptr;
    GameObjectInspector gameObjectInspector;
};

const char PrefabInspector::_register = InspectorRegistry::Register<PrefabInspector, Prefab>();

} // namespace Editor
