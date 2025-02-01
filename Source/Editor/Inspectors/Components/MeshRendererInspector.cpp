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

        bool skinning = meshRenderer->IsSkinningEnabled();
        if (ImGui::Checkbox("Skinning", &skinning))
        {
            skinning = !skinning;

            if (skinning)
            {
                meshRenderer->ValidateSkinning();
            }
            else
            {
                meshRenderer->DisableSkinning();
            }
        }

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
        Object* meshPayload = nullptr;
        if (GUI::DragDropTarget(typeid(Mesh), meshPayload))
        {
            Mesh* mesh = static_cast<Mesh*>(meshPayload);
            meshRenderer->SetMesh(mesh);
            mesh = meshRenderer->GetMesh();
        }

        else if (bp)
        {
            EditorState::SelectObject(mesh->GetSRef());
        }
        // show materials
        ImGui::Text("Materials: ");
        auto mats = meshRenderer->GetMaterials();
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
                EditorState::SelectObject(mats[i]->GetSRef());
            };

            Object* materialPayload;
            if (GUI::DragDropTarget(typeid(Material), materialPayload))
            {

                mats[i] = (Material*)materialPayload;
                meshRenderer->SetMaterials(mats);
            }
        }
    }

private:
    static const char _register;
};

const char MeshRendererInspector::_register = InspectorRegistry::Register<MeshRendererInspector, MeshRenderer>();

} // namespace Editor
