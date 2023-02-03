#include "Light.hpp"

namespace Engine
{
Light::Light() : Component("Light", nullptr)
{
    SERIALIZE_MEMBER(range);
    SERIALIZE_MEMBER(intensity);
}
Light::~Light() {}
Light::Light(GameObject* gameObject) : Component("Light", gameObject)
{

    SERIALIZE_MEMBER(range);
    SERIALIZE_MEMBER(intensity);
}

void Light::SetLightType(LightType type) { this->lightType = type; }
} // namespace Engine
