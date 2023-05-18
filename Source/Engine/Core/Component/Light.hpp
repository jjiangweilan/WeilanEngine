#pragma once
#include "Component.hpp"
#include <glm/glm.hpp>
namespace Engine
{
class GameObject;
enum class LightType
{
    Directional,
    Point
};

class Light : public Component
{
public:
    Light();
    Light(GameObject* gameObject);
    ~Light();

    void SetLightType(LightType type);
    void SetRange(float range) { this->range = range; }
    void SetIntensity(float intensity) { this->intensity = intensity; }
    glm::mat4 WorldToShadowMatrix();

    LightType GetLightType() const { return lightType; }
    float GetRange() const { return range; }
    float GetIntensity() const { return intensity; }

private:
    LightType lightType = LightType::Directional;
    float range; // valid when it's a point light
    float intensity;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Engine
