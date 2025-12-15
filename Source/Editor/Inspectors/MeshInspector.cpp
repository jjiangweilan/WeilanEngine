#include "Editor/EditorState.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
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
        EditorGUI::Text("Name", name.c_str());
        EditorGUI::Text("UUID", target->GetUUID().ToString().c_str());
        ImGui::Spacing();

        int c = 0;
        for (auto& submesh : target->GetSubmeshes())
        {
            EditorGUI::TextFormatted("Submesh", "Submesh-%i", c);
            ImGui::Indent();
            auto& attr = submesh.GetAttribute();
            for (auto& desc : attr.GetDescription())
            {
                EditorGUI::TextFormatted("Attribute", "%s - %i", desc.name.c_str(), desc.size);
            }
            ImGui::Unindent();
            c++;
        }

        if (target->HasSkeleton())
        {
            if (EditorGUI::ButtonSimple("Generate Skeleton"))
            {
            }
        }
    }

private:
    static const char _register;
};

const char MeshInspector::_register = InspectorRegistry::Register<MeshInspector, Mesh>();

} // namespace Editor
