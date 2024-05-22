#include "../Window.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Gizmo.hpp"
#include "ThirdParty/imgui/imgui.h"
class Shader;
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
        ImGui::Checkbox("show grid", &showGrid);
        ImGui::End();

        if (showGrid)
            ShowGrid();

        return open;
    }

    void OnOpen() override
    {
        plane = static_cast<Mesh*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.glb"));
        gridShader = static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/PlaneGrid.shad"));
    }

    void ShowGrid()
    {
        Gizmos::Draw(*plane, 0, gridShader);
    }

    void HideGrid() {}
};

DEFINE_EDITOR_WINDOW(WorldGridModuleEditor, "GameWorld/Tools/WorldGridModule")

} // namespace Editor
