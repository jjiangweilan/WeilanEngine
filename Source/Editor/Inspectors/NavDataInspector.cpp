#include "Editor/Inspectors/Inspector.hpp"
#include "Editor/Inspectors/InspectorRegistry.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"

#include <algorithm>

namespace Editor
{
class NavDataInspector : public Inspector<NavData>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        (void)editor;

        std::string name = target->GetName();
        if (EditorGUI::InputText("Name", name))
        {
            target->SetName(name);
        }

        EditorGUI::Text("UUID", target->GetUUID().ToString());
        EditorGUI::TextFormatted("Cells", "%zu", target->grid.cells.size());
        ImGui::Separator();

        NavDataConfig config = target->grid.config;
        bool configChanged = false;

        configChanged |= EditorGUI::Property("Resolution", config.resolution);
        configChanged |= EditorGUI::Property("Width", config.width);
        configChanged |= EditorGUI::Property("Height", config.height);

        config.resolution = std::max(config.resolution, 0.001f);
        config.width = std::max(config.width, 1);
        config.height = std::max(config.height, 1);

        if (configChanged)
        {
            target->grid.config = config;
            target->grid.cells.clear();
            target->SetDirty();
        }

        if (ImGui::Button("Clear Baked Cells"))
        {
            target->grid.cells.clear();
            target->SetDirty();
        }
    }

private:
    static const char _register;
};

const char NavDataInspector::_register = InspectorRegistry::Register<NavDataInspector, NavData>();
} // namespace Editor
