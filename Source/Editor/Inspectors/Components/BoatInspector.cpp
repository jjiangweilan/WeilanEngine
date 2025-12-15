#include "../Inspector.hpp"
#include "Game/Boat.hpp"
#include "Runtime/Module/Ocean/OceanComponent.hpp"

namespace Editor
{
class BoatInspector : public Inspector<Game::Boat>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Game::Boat>::DrawInspector(editor);

        auto boat = target.Get();
        if (!boat)
            return;
        float initDistance = boat->GetInitDistance();
        if (EditorGUI::DragFloat("Sample Init Distance", &initDistance, 0.01f, 0.01f))
        {
            boat->SetInitDistance(initDistance);
        }
        OceanComponent* ocean = boat->GetOceanComponent();
        if (EditorGUI::ObjectField("Ocean Component", ocean))
        {
            boat->SetOceanComponent(ocean);
        }
    }

private:
    static const char _register;
};
const char BoatInspector::_register = InspectorRegistry::Register<BoatInspector, Game::Boat>();
} // namespace Editor
