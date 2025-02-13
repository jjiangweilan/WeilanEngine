#include "../EditorState.hpp"
#include "Core/Component/GameScript.hpp"
#include "Inspector.hpp"
namespace Editor
{
class GameScriptInspector : public Inspector<GameScript>
{
public:
    void DrawInspector(GameEditor& editor) override {
        LuaScript* script = target->GetScript().Get();
        if(GUI::ObjectField("Lua Script", script))
        {
            target->SetScript(script);
        }

        JsonSerializer s;
        target->Serialize(&s);
        auto j = s.GetJson();
        GUI::JsonInspector(j);
    }

private:
    static const char _register;
};

const char GameScriptInspector::_register = InspectorRegistry::Register<GameScriptInspector, GameScript>();

} // namespace Editor
