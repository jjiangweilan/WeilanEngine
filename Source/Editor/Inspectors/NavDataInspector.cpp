#include "Editor/Inspectors/Inspector.hpp"
#include "Editor/Inspectors/InspectorRegistry.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"

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

        if (configChanged)
        {
            config.resolution.x = std::max(config.resolution.x, 0.001f);
            config.resolution.y = std::max(config.resolution.y, 0.001f);
            target->grid.config = config;
            target->grid.cells.clear();
            target->SetDirty();
        }

        EditorGUI::TextFormatted("Width", "%d", target->grid.config.width);
        EditorGUI::TextFormatted("Height", "%d", target->grid.config.height);
        EditorGUI::TextFormatted("Origin", "%.3f, %.3f, %.3f",
            target->grid.config.origin.x,
            target->grid.config.origin.y,
            target->grid.config.origin.z);

        ImGui::Separator();

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
