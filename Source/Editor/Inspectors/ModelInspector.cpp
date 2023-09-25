#include "../EditorState.hpp"
#include "Core/Model2.hpp"
#include "Inspector.hpp"
namespace Engine::Editor
{
class ModelInspector : public Inspector<Model2>
{
public:
    void DrawInspector() override
    {
        // object information
        auto& name = target->GetName();
        char cname[1024];
        strcpy(cname, name.data());
        if (ImGui::InputText("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        for (auto& mesh : target->GetMeshes())
        {
            if (ImGui::Button(mesh->GetName().c_str()))
            {
                EditorState::selectedObject = mesh.get();
            }

            if (ImGui::BeginDragDropSource())
            {
                Mesh2* ptr = mesh.get();

                ImGui::SetDragDropPayload("object", &ptr, sizeof(ptr));

                ImGui::Text("%s", mesh->GetName().c_str());

                ImGui::EndDragDropSource();
            }
        }
    }

private:
    static const char _register;
};

const char ModelInspector::_register = InspectorRegistry::Register<ModelInspector, Model2>();

} // namespace Engine::Editor
