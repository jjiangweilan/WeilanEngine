#include "Light.hpp"
#include "Core/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
Light::Light() : Component("Light", nullptr)
{
}

Light::Light(GameObject* gameObject) : Component("Light", gameObject)
{
}

Light::~Light() {}

void Light::SetLightType(LightType type) { this->lightType = type; }

glm::mat4 Light::WorldToShadowMatrix()
{
    glm::mat4 proj = glm::ortho(-20., 20., -20., 20., -10., 100.);
    return proj * glm::inverse(gameObject->GetTransform()->GetModelMatrix());
}
} // namespace Engine
