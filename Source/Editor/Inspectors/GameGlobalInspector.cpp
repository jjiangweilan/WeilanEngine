
#include "Engine/Game/Boat.hpp"
#include "Engine/Game/GameGlobal.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{
class GameGlobalInspector : public Inspector<Game::GameGlobal>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Game::GameGlobal* gg = target;
        if (!gg)
            return;

        // Basic header
        EditorGUI::SeparatorTextLabeled("Game Global");

        // Boat reference field
        Game::Boat* boatPtr = gg->boat.Get();
        if (EditorGUI::ObjectField("Boat", boatPtr))
        {
            gg->boat = boatPtr;
        }

        if (boatPtr)
        {
            ImGui::Text("Boat UUID: %s", boatPtr->GetUUID().ToString().c_str());
        }
        else
        {
            ImGui::Text("Boat: None");
        }
    }

private:
    static const char _register;
};

const char GameGlobalInspector::_register = InspectorRegistry::Register<GameGlobalInspector, Game::GameGlobal>();

} // namespace Editor
