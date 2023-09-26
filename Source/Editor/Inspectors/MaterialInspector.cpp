#include "../EditorState.hpp"
#include "Asset/Material.hpp"
#include "Inspector.hpp"
namespace Engine::Editor
{
class MaterialInspector : public Inspector<Material>
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

        auto shader = target->GetShader();

        std::string shaderGUIID = "empty";

        if (shader != nullptr)
            shaderGUIID = shader->GetName();

        bool buttonPressed = ImGui::Button(fmt::format("{}##shader", shaderGUIID).c_str());

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                Object& obj = **(Object**)payload->Data;
                if (typeid(obj) == typeid(Shader))
                {
                    Shader* sourceShader = static_cast<Shader*>(&obj);
                    target->SetShader(sourceShader);
                }
            }
            ImGui::EndDragDropTarget();
        }
        else if (buttonPressed)
        {
            if (shader != nullptr)
            {
                EditorState::selectedObject = shader.Get();
            }
        }
    }

private:
    static const char _register;
};

const char MaterialInspector::_register = InspectorRegistry::Register<MaterialInspector, Material>();

} // namespace Engine::Editor
