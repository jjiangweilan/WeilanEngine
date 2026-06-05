#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Object/Component/MissingComponent.hpp"

namespace Editor
{
class MissingComponentInspector : public Inspector<MissingComponent>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        ImGui::TextWrapped("Component implementation is missing.");
        ImGui::TextWrapped("Original serialized data was discarded during load.");
        ImGui::TextWrapped("Delete this component if it is no longer needed.");
    }

private:
    static const char _register;
};

const char MissingComponentInspector::_register = InspectorRegistry::Register<MissingComponentInspector, MissingComponent>();
} // namespace Editor
