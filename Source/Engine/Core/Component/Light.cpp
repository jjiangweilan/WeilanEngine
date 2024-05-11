#include "Light.hpp"
#include "Core/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>

DEFINE_OBJECT(Light, "DA1910DA-B87F-411E-A8D3-94C5924A23C2");
Light::Light() : Component(nullptr) {}

Light::Light(GameObject* gameObject) : Component(gameObject) {}

Light::~Light() {}

void Light::SetLightType(LightType type)
{
    this->lightType = type;
}

glm::mat4 Light::WorldToShadowMatrix(const glm::vec3& follow)
{
    glm::mat4 proj = glm::ortho(-30., 30., -30., 30., -300., 700.);
    proj[2][2] *= -1;
    auto model = gameObject->GetModelMatrix();
    model[3] = glm::vec4(follow, 1.0);
    return proj * glm::inverse(model);
}

void Light::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("ambientScale", ambientScale);
    s->Serialize("ambient", ambient);
    s->Serialize("range", range);
    s->Serialize("intensity", intensity);
    s->Serialize("pointLightTerm1", pointLightTerm1);
    s->Serialize("pointLightTerm2", pointLightTerm2);
    s->Serialize("lightType", static_cast<int>(lightType));
}
void Light::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("ambientScale", ambientScale);
    s->Deserialize("ambient", ambient);
    s->Deserialize("range", range);
    s->Deserialize("intensity", intensity);
    s->Deserialize("pointLightTerm1", pointLightTerm1);
    s->Deserialize("pointLightTerm2", pointLightTerm2);
    int lightType;
    s->Deserialize("lightType", lightType);
    this->lightType = static_cast<LightType>(lightType);
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

void Light::OnDrawGizmos(Gizmos& gizmos)
{
    gizmos.Add<GizmoLight>(gameObject->GetPosition());
}
