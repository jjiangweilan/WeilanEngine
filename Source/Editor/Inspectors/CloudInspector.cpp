#include "Core/Component/Cloud.hpp"
#include "Inspector.hpp"

namespace Editor
{
class CloudInspector : public Inspector<Cloud>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Cloud>::DrawInspector(editor);

        GUI::AutoObjectInspector(target);

        if (ImGui::Button("Generate Materials"))
        {
            target->CreateCloudMaterials();
        }
    }

private:
    static const char _register;
};

const char CloudInspector::_register = InspectorRegistry::Register<CloudInspector, Cloud>();

} // namespace Editor
