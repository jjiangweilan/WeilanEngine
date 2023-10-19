#include "Light.hpp"
#include "Core/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
DEFINE_OBJECT(Light, "DA1910DA-B87F-411E-A8D3-94C5924A23C2");
Light::Light() : Component(nullptr) {}

Light::Light(GameObject* gameObject) : Component(gameObject) {}

Light::~Light() {}

void Light::SetLightType(LightType type)
{
    this->lightType = type;
}

glm::mat4 Light::WorldToShadowMatrix()
{
    glm::mat4 proj = glm::ortho(-5., 5., -5., 5., -10., 1000.);
    proj[1][1] *= -1;
    proj[2] *= -1;
    return proj * glm::inverse(gameObject->GetTransform()->GetModelMatrix());
}

void Light::Serialize(Serializer* s) const
{

    Component::Serialize(s);
    s->Serialize("range", range);
    s->Serialize("intensity", intensity);
}
void Light::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("range", range);
    s->Deserialize("intensity", intensity);
}

const std::string& Light::GetName()
{
    static std::string name = "Light";
    return name;
}

std::unique_ptr<Component> Light::Clone(GameObject& owner)
{
    auto clone = std::make_unique<Light>(&owner);

    clone->lightType = lightType;
    clone->range = range;
    clone->intensity = intensity;

    return clone;
}
} // namespace Engine
