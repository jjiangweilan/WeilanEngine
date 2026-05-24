#include "../Inspector.hpp"

#include "Editor/EditorGUI.hpp"
#include "Engine/Runtime/Object/Component/Navigation.hpp"

namespace Editor
{
class NavigationInspector : public Inspector<Navigation>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Navigation>::DrawInspector(editor);

        NavData* navData = target->GetNavData().Get();
        if (EditorGUI::ObjectField("Nav Data", navData))
        {
            target->SetNavData(navData);
        }
    }

private:
    static const char _register;
};

const char NavigationInspector::_register = InspectorRegistry::Register<NavigationInspector, Navigation>();
} // namespace Editor
