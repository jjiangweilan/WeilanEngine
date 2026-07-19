#include "Editor/GameEditor.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Runtime/Object/Component/Terrain.hpp"

namespace Editor
{
class TerrainInspector final : public Inspector<Terrain>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        TerrainConfig* config = target->GetTerrainConfig();
        if (EditorGUI::ObjectField("Terrain Config", config))
            target->SetTerrainConfig(config);

        if (config == nullptr)
        {
            ImGui::TextDisabled("Assign a TerrainConfig asset to render and paint terrain.");
            return;
        }

        ImGui::Text("Height map: %u x %u (R16_UNorm)", config->GetHeightMapResolution(), config->GetHeightMapResolution());
        ImGui::Text("Mesh vertices: %u x %u", config->GetVertexResolution(), config->GetVertexResolution());
        if (ImGui::Button("Open Terrain Paint Tool"))
            editor.OpenTerrainPaintWindow(target.Get());

        if (ImGui::TreeNodeEx("Terrain Config Data", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID(config);
            InspectorBase* configInspector = InspectorRegistry::GetInspector(*config);
            configInspector->OnEnable(*config);
            configInspector->DrawInspector(editor);
            ImGui::PopID();
            ImGui::TreePop();
        }
    }

private:
    static const char _register;
};

const char TerrainInspector::_register = InspectorRegistry::Register<TerrainInspector, Terrain>();
} // namespace Editor
