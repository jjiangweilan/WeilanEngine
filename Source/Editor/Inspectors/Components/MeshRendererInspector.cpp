#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/MeshRenderer.hpp"

namespace Editor
{
class MeshRendererInspector : public Inspector<MeshRenderer>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        MeshRenderer* meshRenderer = target;

        Mesh* mesh = meshRenderer->GetMesh();

        std::string meshGUIID = "emtpy";
        if (mesh)
            meshGUIID = mesh->GetName();

        bool multipass = meshRenderer->IsMultipassEnabled();
        if (ImGui::Checkbox("Multipass", &multipass))
        {
            if (multipass)
                meshRenderer->EnableMultipass();
            else
                meshRenderer->DisableMultipass();
        }

        if (multipass)
        {
            int size = meshRenderer->GetMaterialSize();
            if (ImGui::InputInt("Material Size", &size))
            {
                if (meshRenderer->GetMaterialSize() != size)
                {
                    meshRenderer->SetMaterialSize(size);
                }
            }
        }

        ImGui::Text("Mesh: ");
        ImGui::SameLine();

        bool bp = ImGui::Button(meshGUIID.c_str());
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                Object& obj = **(Object**)payload->Data;
                if (typeid(obj) == typeid(Mesh))
                {
                    Mesh* mesh = static_cast<Mesh*>(&obj);

                    meshRenderer->SetMesh(mesh);
                    mesh = meshRenderer->GetMesh();
                }
            }
            ImGui::EndDragDropTarget();
        }
        else if (bp)
        {
            EditorState::selectedObject = mesh->GetSRef();
        }
        // show materials
        ImGui::Text("Materials: ");
        std::vector<Material*> mats = meshRenderer->GetMaterials();
        for (int i = 0; i < mats.size(); ++i)
        {
            Material* mat = mats[i];
            std::string buttonID;
            if (mat)
                buttonID = fmt::format("{}: {}", i, mats[i]->GetName());
            else
                buttonID = fmt::format("{}##{}", "emtpy", i);

            if (ImGui::Button(buttonID.c_str()))
            {
                EditorState::selectedObject = mats[i]->GetSRef();
            };

            if (ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
                if (payload && payload->IsDelivery())
                {
                    Object& obj = **(Object**)payload->Data;
                    if (typeid(obj) == typeid(Material))
                    {
                        mats[i] = static_cast<Material*>(&obj);
                        meshRenderer->SetMaterials(mats);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

private:
    static const char _register;
};

const char MeshRendererInspector::_register = InspectorRegistry::Register<MeshRendererInspector, MeshRenderer>();

} // namespace Editor
