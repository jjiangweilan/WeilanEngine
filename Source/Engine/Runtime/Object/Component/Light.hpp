#pragma once
#include "Component.hpp"
#include "Engine/Library/ColorSpace.hpp"
#include <glm/glm.hpp>
class GameObject;
class Texture;
enum class LightType
{
    Directional,
    Point
};

struct ShadowCascade
{
    float splitDistance = 0.0f;
    // float fadeStart = 0.8f;
};

class Light : public Component
{
    DECLARE_OBJECT();

    float directionalLightFrustum_W[6] = {-10, 10, -10, 10, -300, 700};
    LightType lightType = LightType::Directional;
    glm::vec4 lightColor = glm::vec4(1, 1, 1, 1);
    glm::vec4 linearLightColor = glm::vec4(1, 1, 1, 1);
    glm::vec4 skyColor = glm::vec4(0.3f, 0.5f, 0.85f, 1.0f);
    glm::vec4 skyHorizonFalloffColor = glm::vec4(0.7f, 0.75f, 0.85f, 1.0f);
    glm::vec4 skyHorizonColor = glm::vec4(0.418f, 0.394f, 0.372f, 1.0f);
    glm::vec4 skySunColor = glm::vec4(1.0f, 0.7f, 0.4f, 1.0f);
    glm::vec4 skySunCoreColor = glm::vec4(1.0f, 0.8f, 0.6f, 1.0f);
    float skyboxIntensity = 1.0f;
    bool useHDRISkybox = false;
    ObjPtr<Texture> hdriSkybox = nullptr;
    float hdriSkyboxRotationDegrees = 0.0f;
    float ambientScale = 1.0f;
    float range = 10.0f; // valid when it's a point light
    float intensity = 1.0f;
    float pointLightTerm1 = 0.7f;
    float pointLightTerm2 = 1.8f;
    float shadowDistance = 100.0;
    bool enableCascadedShadow = true;
    bool enablePointLightShadow = true;
    std::vector<ShadowCascade> shadowCascades = {};

    struct
    {
        bool isEnabled = false;
        int frames = 0;
        int targetFrames = 0;
        glm::vec3 cachedLightDirection;
        glm::mat4 cachedWorldToShadow = glm::mat4(1.0f);

    } shadowCache;

public:
    float depthBias = -1.0f;
    float depthSlopeBias = -3.0f;

public:
    Light();
    Light(GameObject* gameObject);
    ~Light();

    void SetLightType(LightType type);
    void SetRange(float range) { this->range = range; }
    void SetIntensity(float intensity) { this->intensity = intensity; }

    [[deprecated("Implementation moved to ShadowRenderer")]]
    glm::mat4 WorldToShadowMatrix(const glm::vec3& follow);

    LightType GetLightType() const { return lightType; }

    float GetRange() const { return range; }
    float GetIntensity() const { return intensity; }

    glm::vec3 GetLightColor() const { return lightColor; }
    glm::vec3 GetLinearLightColor() const { return linearLightColor; }
    glm::vec3 GetSkyColor() const { return skyColor; }
    glm::vec3 GetSkyHorizonFalloffColor() const { return skyHorizonFalloffColor; }
    glm::vec3 GetSkyHorizonColor() const { return skyHorizonColor; }
    glm::vec3 GetSkySunColor() const { return skySunColor; }
    glm::vec3 GetSkySunCoreColor() const { return skySunCoreColor; }
    glm::vec3 GetLinearSkyColor() const { return ColorSpace::SRGBToLinear(glm::vec3(skyColor)); }
    glm::vec3 GetLinearSkyHorizonFalloffColor() const
    {
        return ColorSpace::SRGBToLinear(glm::vec3(skyHorizonFalloffColor));
    }
    glm::vec3 GetLinearSkyHorizonColor() const { return ColorSpace::SRGBToLinear(glm::vec3(skyHorizonColor)); }
    glm::vec3 GetLinearSkySunColor() const { return ColorSpace::SRGBToLinear(glm::vec3(skySunColor)); }
    glm::vec3 GetLinearSkySunCoreColor() const { return ColorSpace::SRGBToLinear(glm::vec3(skySunCoreColor)); }
    float GetSkyboxIntensity() const { return skyboxIntensity; }
    bool IsHDRISkyboxEnabled() const { return useHDRISkybox; }
    const ObjPtr<Texture>& GetHDRISkybox() const { return hdriSkybox; }
    float GetHDRISkyboxRotationDegrees() const { return hdriSkyboxRotationDegrees; }

    float GetAmbientScale() const { return ambientScale; }

    void SetAmbientScale(float scale) { this->ambientScale = scale; }
    void SetSkyboxIntensity(float intensity) { skyboxIntensity = intensity; }
    void SetHDRISkyboxEnabled(bool enabled) { useHDRISkybox = enabled; }
    void SetHDRISkybox(Texture* texture);
    void SetHDRISkyboxRotationDegrees(float rotationDegrees) { hdriSkyboxRotationDegrees = rotationDegrees; }

    void SetLightColor(glm::vec3 lightColor)
    {
        this->lightColor = glm::vec4(lightColor, 1.0);
        this->linearLightColor = glm::vec4(ColorSpace::SRGBToLinear(lightColor), 1.0f);
    }

    void SetSkyColor(glm::vec3 color) { skyColor = glm::vec4(color, 1.0f); }
    void SetSkyHorizonFalloffColor(glm::vec3 color) { skyHorizonFalloffColor = glm::vec4(color, 1.0f); }
    void SetSkyHorizonColor(glm::vec3 color) { skyHorizonColor = glm::vec4(color, 1.0f); }
    void SetSkySunColor(glm::vec3 color) { skySunColor = glm::vec4(color, 1.0f); }
    void SetSkySunCoreColor(glm::vec3 color) { skySunCoreColor = glm::vec4(color, 1.0f); }

    void SetPointLightLinear(float t) { this->pointLightTerm1 = t; }

    void SetPointLightDistance(float t) { this->pointLightTerm2 = t; }

    float GetPointLightLinear() { return pointLightTerm1; }

    float GetPointLightDistance() { return pointLightTerm2; }

    glm::vec3 GetLightDirection();
    float GetMainLightNearPlane() { return directionalLightFrustum_W[4]; }
    float GetMainLightFarPlane() { return directionalLightFrustum_W[5]; }
    float GetShadowDistance() { return shadowDistance; }
    void SetShadowDistance(float shadowDistance) { this->shadowDistance = shadowDistance; }

    glm::vec3 GetCachedLightDirection() { return shadowCache.cachedLightDirection; }

    float GetShadowPlane() { return 100; }

    void OnAwake() override;
    void SetShadowCascadeEnabled(bool enabled);
    void SetCascadeShadowSplits(const std::vector<ShadowCascade>& cascades) { shadowCascades = cascades; }
    const std::vector<ShadowCascade>& GetShadowCascadeSplits() const { return shadowCascades; }
    bool IsCascadeShadowEnabled() const { return enableCascadedShadow; }
    bool IsPointLightShadowEnabled() const { return enablePointLightShadow; }
    void SetPointLightShadowEnabled(bool enabled) { enablePointLightShadow = enabled; }
    int GetCascadeCount() const { return static_cast<int>(shadowCascades.size()); }

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() const override;

    void EnableShadowCache()
    {
        shadowCache.isEnabled = true;
        shadowCache.cachedLightDirection = GetLightDirection();
        shadowCache.frames = 0;
        shadowCache.targetFrames = 8;
    }

    void DisableShadowCache()
    {
        shadowCache.isEnabled = false;
        shadowCache.frames = 0;
    }

    bool IsShadowCacheEnabled() { return shadowCache.isEnabled; }

    bool ShouldRenderShadowMap() { return !shadowCache.isEnabled || shadowCache.frames == 0; }

    void SetShadowUpdateFrames(int frames) { shadowCache.targetFrames = frames; }

    int GetShadowCacheTargetFrames() { return shadowCache.targetFrames; }

    void Tick() override
    {
        if (shadowCache.isEnabled)
        {
            shadowCache.frames = (shadowCache.frames + 1) % shadowCache.targetFrames;
        }
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnDrawGizmos() override;
};
