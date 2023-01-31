#pragma once
#include "Component.hpp"

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
    DECLARE_COMPONENT(Light);

public:
    Light();
    Light(GameObject* gameObject);
    ~Light();

    void SetLightType(LightType type);
    void SetRange(float range) { this->range = range; }
    void SetIntensity(float intensity) { this->intensity = intensity; }

    LightType GetLightType() const { return lightType; }
    float GetRange() const { return range; }
    float GetIntensity() const { return intensity; }

private:
    LightType lightType = LightType::Point;
    float range; // valid when it's a point light
    float intensity;
};
} // namespace Engine
