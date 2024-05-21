#include "../Window.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Editor
{
class WorldGridModuleEditor : public Window
{
    DECLARE_EDITOR_WINDOW(WorldGridModuleEditor)
public:
    bool Tick() override
    {
        bool open = true;
        ImGui::Begin("World Grid Module", &open);

        ImGui::Text("hello window");

        ImGui::End();
        return open;
    }
};

DEFINE_EDITOR_WINDOW(WorldGridModuleEditor, "GameWorld/Tools/WorldGridModule")

} // namespace Editor
