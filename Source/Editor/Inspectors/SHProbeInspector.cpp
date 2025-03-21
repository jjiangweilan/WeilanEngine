#include "Inspector.hpp"
#include "Rendering/SHProbe.hpp"

namespace Editor
{
class SHProbeInspector : public Inspector<SHProbe>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        if (ImGui::Button("Bake"))
        {
            target->UpdateProbe({});
        }

        target->UpdateProbe({});
    }

private:
    static const char _register;
};

const char SHProbeInspector::_register = InspectorRegistry::Register<SHProbeInspector, SHProbe>();
} // namespace Editor
