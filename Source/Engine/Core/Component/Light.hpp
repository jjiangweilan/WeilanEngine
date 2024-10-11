#pragma once
#include "Component.hpp"
#include <glm/glm.hpp>
class GameObject;
enum class LightType
{
    Directional,
    Point
};

class Light : public Component
{
    DECLARE_OBJECT();

public:
    Light();
    Light(GameObject* gameObject);
    ~Light();

    void SetLightType(LightType type);
    void SetRange(float range)
    {
        this->range = range;
    }
    void SetIntensity(float intensity)
    {
        this->intensity = intensity;
    }
    glm::mat4 WorldToShadowMatrix(const glm::vec3& follow);

    LightType GetLightType() const
    {
        return lightType;
    }

    float GetRange() const
    {
        return range;
    }
    float GetIntensity() const

    {
        return intensity;
    }

    glm::vec3 GetLightColor() const
    {
        return ambient;
    }

    float GetAmbientScale() const
    {
        return ambientScale;
    }

    void SetAmbientScale(float scale)
    {
        this->ambientScale = scale;
    }

    void SetLightColor(glm::vec3 ambient)
    {
        this->ambient = glm::vec4(ambient, 1.0);
    }

    void SetPointLightLinear(float t)
    {
        this->pointLightTerm1 = t;
    }

    void SetPointLightDistance(float t)
    {
        this->pointLightTerm2 = t;
    }

    float GetPointLightLinear()
    {
        return pointLightTerm1;
    }

    float GetPointLightDistance()
    {
        return pointLightTerm2;
    }

    glm::vec3 GetLightDirection();

    glm::vec3 GetCachedLightDirection()
    {
        return shadowCache.cachedLightDirection;
    }

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

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

    bool IsShadowCacheEnabled()
    {
        return shadowCache.isEnabled;
    }

    bool ShouldRenderShadowMap()
    {
        return !shadowCache.isEnabled || shadowCache.frames == 0;
    }

    void SetShadowUpdateFrames(int frames)
    {
        shadowCache.targetFrames = frames;
    }

    int GetShadowCacheTargetFrames()
    {
        return shadowCache.targetFrames;
    }

    void Tick() override
    {
        if (shadowCache.isEnabled)
        {
            shadowCache.frames = (shadowCache.frames + 1) % shadowCache.targetFrames;
        }
    }

private:
    LightType lightType = LightType::Directional;
    glm::vec4 ambient = glm::vec4(1, 1, 1, 1);
    float ambientScale = 1.0f;
    float range = 10.0f; // valid when it's a point light
    float intensity = 1.0f;
    float pointLightTerm1 = 0.7f;
    float pointLightTerm2 = 1.8f;

    struct
    {
        bool isEnabled = false;
        int frames = 0;
        int targetFrames = 0;
        glm::vec3 cachedLightDirection;
        glm::mat4 cachedWorldToShadow = glm::mat4(1.0f);

    } shadowCache;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnDrawGizmos() override;
};
