#include "LightFieldProbes.hpp"

DEFINE_OBJECT(LightFieldProbes, "EA1F3D6D-9016-4BD0-8062-BA88D022BB50");

LightFieldProbes::LightFieldProbes() : Component(nullptr) {}
LightFieldProbes::LightFieldProbes(GameObject* gameObject) : Component(gameObject){};

void LightFieldProbes::EnableImple() {}
void LightFieldProbes::DisableImple() {}

const std::string& LightFieldProbes::GetName()
{
    static std::string name = "LightFieldProbes;";
    return name;
}

std::unique_ptr<Component> LightFieldProbes::Clone(GameObject& owner)
{
    return nullptr;
}
