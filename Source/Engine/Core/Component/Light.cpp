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
    if (shadowCache.isEnabled && !(shadowCache.frames == 0))
    {
        return shadowCache.cachedWorldToShadow;
    }
    else
    {
        glm::mat4 proj = glm::orthoLH_ZO(-30., 30., -30., 30., -300., 700.);
        proj[1] *= -1;
        auto model = gameObject->GetWorldMatrix();
        model[3] = glm::vec4(follow, 1.0);
        shadowCache.cachedWorldToShadow = proj * glm::inverse(model);
        shadowCache.cachedLightDirection = GetLightDirection();
        return shadowCache.cachedWorldToShadow;
    }
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

void Light::OnDrawGizmos()
{
    Gizmos::DrawLight(gameObject->GetPosition());
}

glm::vec3 Light::GetLightDirection()
{
    auto model = GetGameObject()->GetWorldMatrix();
    glm::vec3 pos = -glm::normalize(glm::vec3(model[2]));
    return pos;
}
