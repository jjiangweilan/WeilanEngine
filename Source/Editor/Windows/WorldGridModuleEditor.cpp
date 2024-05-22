#include "../Window.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Gizmo.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Editor
{
class WorldGridModuleEditor : public Window
{
    DECLARE_EDITOR_WINDOW(WorldGridModuleEditor)
    bool showGrid;
    Mesh* plane;
    Shader* gridShader;

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

    void OnOpen() override{
        plane = static_cast<Mesh*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.glb"));
    }

    void ShowGrid() {
        Gizmos::Draw(*plane, 0, gridShader);
    }

    void HideGrid() {}
};

DEFINE_EDITOR_WINDOW(WorldGridModuleEditor, "GameWorld/Tools/WorldGridModule")

} // namespace Editor
