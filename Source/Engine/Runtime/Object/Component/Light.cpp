#include "Light.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>

DEFINE_OBJECT(Component, Light, "DA1910DA-B87F-411E-A8D3-94C5924A23C2");

TYPE_REFLECTION_MEMBER_VARIABLES(
    Light,
    TYPE_REFLECTION_MEM(Light, ambientScale),
    TYPE_REFLECTION_MEM(Light, lightColor),
    TYPE_REFLECTION_MEM(Light, skyColor),
    TYPE_REFLECTION_MEM(Light, skyHorizonFalloffColor),
    TYPE_REFLECTION_MEM(Light, skyHorizonColor),
    TYPE_REFLECTION_MEM(Light, skySunColor),
    TYPE_REFLECTION_MEM(Light, skySunCoreColor),
    TYPE_REFLECTION_MEM(Light, skyboxIntensity),
    TYPE_REFLECTION_MEM(Light, range),
    TYPE_REFLECTION_MEM(Light, intensity),
    TYPE_REFLECTION_MEM(Light, pointLightTerm1),
    TYPE_REFLECTION_MEM(Light, pointLightTerm2),
    TYPE_REFLECTION_MEM(Light, lightType),
    TYPE_REFLECTION_MEM(Light, shadowDistance),
    TYPE_REFLECTION_MEM(Light, depthBias),
    TYPE_REFLECTION_MEM(Light, depthSlopeBias),
    TYPE_REFLECTION_MEM(Light, enablePointLightShadow)
);
Light::Light() : Component(nullptr) {}

Light::Light(GameObject* gameObject) : Component(gameObject) {}

Light::~Light() {}

void Light::SetLightType(LightType type)
{
    this->lightType = type;
}

void Light::OnAwake()
{
    SetShadowCascadeEnabled(IsCascadeShadowEnabled());
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
        proj[1] = -proj[1];
        auto model = gameObject->GetWorldMatrix();
        model[2] = -model[2];
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
    s->Serialize("ambient", lightColor);
    s->Serialize("skyColor", skyColor);
    s->Serialize("skyHorizonFalloffColor", skyHorizonFalloffColor);
    s->Serialize("skyHorizonColor", skyHorizonColor);
    s->Serialize("skySunColor", skySunColor);
    s->Serialize("skySunCoreColor", skySunCoreColor);
    s->Serialize("skyboxIntensity", skyboxIntensity);
    s->Serialize("range", range);
    s->Serialize("intensity", intensity);
    s->Serialize("pointLightTerm1", pointLightTerm1);
    s->Serialize("pointLightTerm2", pointLightTerm2);
    s->Serialize("lightType", static_cast<int>(lightType));
    s->Serialize("shadowDistance", shadowDistance);
    s->Serialize("depthBias", depthBias);
    s->Serialize("depthSlopeBias", depthSlopeBias);
    s->Serialize("enablePointLightShadow", enablePointLightShadow);
    s->Serialize("enableCascadedShadow", enableCascadedShadow);
    s->Serialize("cascadeCount", static_cast<int>(shadowCascades.size()));
    for (size_t i = 0; i < shadowCascades.size(); ++i)
    {
        std::string key = "cascade" + std::to_string(i) + "SplitDistance";
        s->Serialize(key.c_str(), shadowCascades[i].splitDistance);
    }
}
void Light::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("ambientScale", ambientScale);
    s->Deserialize("ambient", lightColor);
    SetLightColor(lightColor);
    s->Deserialize("skyColor", skyColor);
    s->Deserialize("skyHorizonFalloffColor", skyHorizonFalloffColor);
    s->Deserialize("skyHorizonColor", skyHorizonColor);
    s->Deserialize("skySunColor", skySunColor);
    s->Deserialize("skySunCoreColor", skySunCoreColor);
    s->Deserialize("skyboxIntensity", skyboxIntensity);
    s->Deserialize("range", range);
    s->Deserialize("intensity", intensity);
    s->Deserialize("pointLightTerm1", pointLightTerm1);
    s->Deserialize("pointLightTerm2", pointLightTerm2);
    int lightType;
    s->Deserialize("lightType", lightType);
    this->lightType = static_cast<LightType>(lightType);
    s->Deserialize("shadowDistance", shadowDistance);
    s->Deserialize("depthBias", depthBias);
    s->Deserialize("depthSlopeBias", depthSlopeBias);
    s->Deserialize("enablePointLightShadow", enablePointLightShadow);
    s->Deserialize("enableCascadedShadow", enableCascadedShadow);
    int cascadeCount = 0;
    s->Deserialize("cascadeCount", cascadeCount);
    shadowCascades.clear();
    for (int i = 0; i < cascadeCount; ++i)
    {
        std::string key = "cascade" + std::to_string(i) + "SplitDistance";
        float splitDistance = 0.0f;
        s->Deserialize(key.c_str(), splitDistance);
        shadowCascades.push_back({splitDistance});
    }
}

const std::string& Light::GetName() const
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
    clone->skyColor = skyColor;
    clone->skyHorizonFalloffColor = skyHorizonFalloffColor;
    clone->skyHorizonColor = skyHorizonColor;
    clone->skySunColor = skySunColor;
    clone->skySunCoreColor = skySunCoreColor;
    clone->skyboxIntensity = skyboxIntensity;

    return clone;
}

void Light::OnDrawGizmos()
{
    Gizmos::DrawLight(gameObject->GetPosition());
}

glm::vec3 Light::GetLightDirection()
{
    auto model = GetGameObject()->GetWorldMatrix();
    glm::vec3 pos = glm::normalize(glm::vec3(model[2]));
    return -pos;
}

void Light::SetShadowCascadeEnabled(bool enabled)
{
    enableCascadedShadow = enabled;

    if (enabled && shadowCascades.empty())
    {
        shadowCascades.push_back({50.0f});
        shadowCascades.push_back({150.0f});
        shadowCascades.push_back({250.0f});
    }
}
