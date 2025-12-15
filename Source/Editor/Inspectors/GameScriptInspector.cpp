#include "Editor/EditorState.hpp"
#include "Runtime/Object/Component/GameScript.hpp"
#include "Inspector.hpp"
namespace Editor
{
class GameScriptInspector : public Inspector<GameScript>
{
public:
    void DrawInspector(GameEditor& editor) override {
        LuaScript* script = target->GetScript().Get();
        if(EditorGUI::ObjectField("Lua Script", script))
        {
            target->SetScript(script);
        }

        JsonSerializer s;
        target->LuaSerialize(&s);
        auto j = s.GetJson();
        bool valueChanged = false;
        EditorGUI::JsonInspector(j, valueChanged);
        if (valueChanged)
        {
            JsonSerializer des(j);
            target->LuaDeserialize(&des);
        }
    }

private:
    static const char _register;
};

const char GameScriptInspector::_register = InspectorRegistry::Register<GameScriptInspector, GameScript>();

} // namespace Editor
