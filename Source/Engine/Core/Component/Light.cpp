#include "Light.hpp"

namespace Engine
{
Light::Light() : Component("Light", nullptr) {}
Light::~Light() {}
Light::Light(GameObject* gameObject) : Component("Light", gameObject) {}

void Light::SetLightType(LightType type) { this->lightType = type; }
} // namespace Engine
