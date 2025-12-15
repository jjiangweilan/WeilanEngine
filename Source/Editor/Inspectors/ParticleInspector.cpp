#include "Runtime/Object/Component/ParticleSystem.hpp"
#include "Inspector.hpp"
namespace Editor
{
class ParticleSystemInspector : public Inspector<ParticleSystem>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        int particleCount = target->GetParticleCount();
        if (ImGui::InputInt("Particle Count", &particleCount))
        {
            target->SetParticleCount(static_cast<int>(particleCount));
        }
    }

private:
    static const char _register;
};

const char ParticleSystemInspector::_register = InspectorRegistry::Register<ParticleSystemInspector, ParticleSystem>();

} // namespace Editor
