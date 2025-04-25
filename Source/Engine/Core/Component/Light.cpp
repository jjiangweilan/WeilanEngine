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
        glm::mat4 proj = glm::orthoLH_ZO(
            directionalLightFrustum_W[0],
            directionalLightFrustum_W[1],
            directionalLightFrustum_W[2],
            directionalLightFrustum_W[3],
            directionalLightFrustum_W[4],
            directionalLightFrustum_W[5]
        );
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

void Light::GetLightFrusutmPlanes(glm::float4 frustumPlanes[6], const float3& follow)
{
    auto worldToShadow = WorldToShadowMatrix(follow);

    auto row0 = glm::row(worldToShadow, 0);
    auto row1 = glm::row(worldToShadow, 1);
    auto row2 = glm::row(worldToShadow, 2);
    auto row3 = glm::row(worldToShadow, 3);

    // Left plane
    frustumPlanes[0] = row0 + row0;
    // Right plane
    frustumPlanes[1] = row3 - row0;
    // Bottom plane
    frustumPlanes[2] = row3 + row1;
    // Top plane
    frustumPlanes[3] = row3 - row1;
    // Near plane
    frustumPlanes[4] = row2; // z ranges from 0 to 1
    // Far plane
    frustumPlanes[5] = row3 - row2;

    for (int i = 0; i < 6; ++i)
    {
        float length = glm::length(glm::vec3(frustumPlanes[i]));
        frustumPlanes[i] /= length;
    }
}
