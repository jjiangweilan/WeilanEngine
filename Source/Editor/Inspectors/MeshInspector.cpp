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
        ImGui::Text("name: %s", name.c_str());
        ImGui::Text("UUID: %s", target->GetUUID().ToString().c_str());
    }

private:
    static const char _register;
};

const char MeshInspector::_register = InspectorRegistry::Register<MeshInspector, Mesh>();

} // namespace Engine::Editor
