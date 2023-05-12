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

    friend struct SerializableField<Light>;
};

template <>
struct SerializableField<Light>
{
    static void Serialize(Light* v, Serializer* s)
    {
        SerializableField<Component>::Serialize(v, s);
        s->Serialize(v->range);
        s->Serialize(v->intensity);
    }

    static void Deserialize(Light* v, Serializer* s)
    {
        SerializableField<Component>::Deserialize(v ,s);
        s->Deserialize(v->range);
        s->Deserialize(v->intensity);
    }
};
} // namespace Engine
