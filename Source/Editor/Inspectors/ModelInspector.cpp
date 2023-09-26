#include "../EditorState.hpp"
#include "Core/Model.hpp"
#include "Inspector.hpp"
namespace Engine::Editor
{
class ModelInspector : public Inspector<Model>
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
            bool bp = ImGui::Button(mesh->GetName().c_str());
            if (ImGui::BeginDragDropSource())
            {
                Mesh* ptr = mesh.get();

                ImGui::SetDragDropPayload("object", &ptr, sizeof(void*));

                ImGui::Text("%s", mesh->GetName().c_str());

                ImGui::EndDragDropSource();
            }
            else if (bp)
            {
                EditorState::selectedObject = mesh.get();
            }
        }
    }

private:
    static const char _register;
};

const char ModelInspector::_register = InspectorRegistry::Register<ModelInspector, Model>();

} // namespace Engine::Editor
