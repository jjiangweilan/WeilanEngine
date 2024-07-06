#include "LightFieldProbes.hpp"
#include "Core/GameObject.hpp"
DEFINE_OBJECT(LightFieldProbes, "EA1F3D6D-9016-4BD0-8062-BA88D022BB50");

LightFieldProbes::LightFieldProbes() : Component(nullptr) {}
LightFieldProbes::LightFieldProbes(GameObject* gameObject) : Component(gameObject){};

void LightFieldProbes::EnableImple() {}
void LightFieldProbes::DisableImple() {}

const std::string& LightFieldProbes::GetName()
{
    static std::string name = "LightFieldProbes";
    return name;
}

std::unique_ptr<Component> LightFieldProbes::Clone(GameObject& owner)
{
    return nullptr;
}

void LightFieldProbes::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("probeCount", probeCount);
    s->Serialize("gridMin", gridMin);
    s->Serialize("gridMax", gridMax);
}
void LightFieldProbes::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("probeCount", probeCount);
    s->Deserialize("gridMin", gridMin);
    s->Deserialize("gridMax", gridMax);
}

void LightFieldProbes::BakeProbeCubemaps(bool debug)
{
    lfp.PlaceProbes(GetGameObject()->GetPosition(), gridMin, gridMax, probeCount);
    lfp.BakeProbeGBuffers(this->GetScene(), debug);
}
