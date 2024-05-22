#include "../Window.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Editor
{
class WorldGridModuleEditor : public Window
{
    DECLARE_EDITOR_WINDOW(WorldGridModuleEditor)
    bool showGrid;

    bool Tick() override
    {
        bool open = true;
        ImGui::Begin("World Grid Module", &open);
        if (ImGui::Checkbox("show grid", &showGrid))
        {
            if (showGrid)
                ShowGrid();
            else
                HideGrid();
        }

        ImGui::End();
        return open;
    }

    void ShowGrid() {}

    void HideGrid() {}
};

DEFINE_EDITOR_WINDOW(WorldGridModuleEditor, "GameWorld/Tools/WorldGridModule")

} // namespace Editor
