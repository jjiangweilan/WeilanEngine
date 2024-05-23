#include "../Window.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Gizmo.hpp"
#include "Core/Model.hpp"
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
    glm::vec3 pos = glm::vec3(0);
    glm::vec3 scale = glm::vec3(10);
    glm::vec3 rotation;

    bool Tick() override
    {
        bool open = true;
        ImGui::Begin("World Grid Module", &open);
        ImGui::Checkbox("show grid", &showGrid);
        ImGui::DragFloat3("pos", &pos[0]);
        ImGui::DragFloat3("scale", &scale[0]);
        ImGui::End();

        if (showGrid)
            ShowGrid();

        return open;
    }

    void OnOpen() override
    {
        plane = static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Plane.glb"))
                    ->GetMeshes()[0]
                    .get();
        gridShader =
            static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/PlaneGrid.shad"));
    }

    void ShowGrid()
    {
        Gizmos::DrawMesh(*plane, 0, gridShader, glm::scale(glm::translate(glm::mat4(1), pos), scale));
    }

    void HideGrid() {}
};

DEFINE_EDITOR_WINDOW(WorldGridModuleEditor, "GameWorld/Tools/WorldGridModule")

} // namespace Editor
