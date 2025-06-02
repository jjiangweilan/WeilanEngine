#include "ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"

DEFINE_OBJECT(ReflectionProbe, "E93609AC-6D6C-4F13-9538-CD63E2D58567")

ReflectionProbe::ReflectionProbe() : Component(nullptr) {}
ReflectionProbe::ReflectionProbe(GameObject* go) : Component(go) {}
ReflectionProbe::~ReflectionProbe() {}
const std::string& ReflectionProbe::GetName()
{
    static std::string name = "ReflectionProbe";
    return name;
}

void ReflectionProbe::OnEnable()
{
    auto scene = GetScene();
    if (scene)
    {
        scene->GetRenderingScene().AddRenderObject(*this);
    }
}
void ReflectionProbe::OnDisable()
{
    auto scene = GetScene();
    if (scene)
    {
        scene->GetRenderingScene().RemoveRenderObject(*this);
    }
}
