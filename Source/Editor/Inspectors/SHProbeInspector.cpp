#include "Inspector.hpp"
#include "InspectorHelper_Image.hpp"
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
            imageInspector.Initialize(target->cubeMap.get());
        }
    }

private:
    ImageInspector imageInspector;

    static const char _register;
};

const char SHProbeInspector::_register = InspectorRegistry::Register<SHProbeInspector, SHProbe>();
} // namespace Editor
