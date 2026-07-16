#include "Editor/Inspectors/Inspector.hpp"
#include "Editor/Inspectors/InspectorRegistry.hpp"
#include "Engine/Core/BinaryAsset.hpp"

namespace Editor
{
class BinaryAssetInspector : public Inspector<BinaryAsset>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        (void)editor;

        std::string name = target->GetName();
        if (EditorGUI::InputText("Name", name))
            target->SetName(name);

        EditorGUI::Text("UUID", target->GetUUID().ToString());
        EditorGUI::TextFormatted("Bytes", "%zu", target->GetSize());
    }

private:
    static const char _register;
};

const char BinaryAssetInspector::_register = InspectorRegistry::Register<BinaryAssetInspector, BinaryAsset>();
} // namespace Editor
