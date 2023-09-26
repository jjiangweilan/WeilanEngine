#include "../EditorState.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Inspector.hpp"
namespace Engine::Editor
{
class MeshInspector : public Inspector<Mesh>
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
    }

private:
    static const char _register;
};

const char MeshInspector::_register = InspectorRegistry::Register<MeshInspector, Mesh>();

} // namespace Engine::Editor
