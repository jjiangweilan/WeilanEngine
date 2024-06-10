#include "../EditorState.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Inspector.hpp"
namespace Editor
{
class MeshInspector : public Inspector<Mesh>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        // object information
        auto& name = target->GetName();
        ImGui::Text("name: %s", name.c_str());
        ImGui::Text("UUID: %s", target->GetUUID().ToString().c_str());
        ImGui::Spacing();

        int c = 0;
        for (auto& submesh : target->GetSubmeshes())
        {
            ImGui::Text("Submesh-%i", c);
            ImGui::Indent();
            auto& attr = submesh.GetAttribute();
            for (auto& desc : attr.GetDescription())
            {
                ImGui::Text("%s - %i", desc.name.c_str(), desc.size);
            }
            ImGui::Unindent();
            c++;
        }
    }

private:
    static const char _register;
};

const char MeshInspector::_register = InspectorRegistry::Register<MeshInspector, Mesh>();

} // namespace Editor
