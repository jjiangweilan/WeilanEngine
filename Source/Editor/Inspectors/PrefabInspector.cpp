#include "Core/Prefab.hpp"
#include "EditorGUI.hpp"
#include "Inspector.hpp"

namespace Editor
{
class PrefabInspector : public Inspector<Prefab>
{
public:
    void DrawInspector(GameEditor& editor) override {
        
        ImGui::SeparatorText("GameObject");

        GUI::AutoObjectInspect(target->GetGameObject());
    }

private:
    static const char _register;
};

const char PrefabInspector::_register = InspectorRegistry::Register<PrefabInspector, Prefab>();

} // namespace Editor
