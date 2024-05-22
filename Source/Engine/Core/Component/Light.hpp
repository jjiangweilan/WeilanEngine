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

    void SetPointLightQuadratic(float t)
    {
        this->pointLightTerm2 = t;
    }

    float GetPointLightLinear()
    {
        return pointLightTerm1;
    }

    float GetPointLightQuadratic()
    {
        return pointLightTerm2;
    }

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    LightType lightType = LightType::Directional;
    glm::vec4 ambient = glm::vec4(1, 1, 1, 1);
    float ambientScale = 1.0f;
    float range = 10.0f; // valid when it's a point light
    float intensity = 1.0f;
    float pointLightTerm1 = 0.7f;
    float pointLightTerm2 = 1.8f;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnDrawGizmos() override;
};
