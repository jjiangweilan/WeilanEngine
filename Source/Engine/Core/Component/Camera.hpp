#pragma once

#include "Component.hpp"
#include "Core/Math/Geometry.hpp"
#include <glm/glm.hpp>
namespace Engine
{
class Camera : public Component
{
public:
    Camera();
    Camera(GameObject* gameObject);
    ~Camera() override{};
    glm::mat4 GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    glm::vec3 ScreenUVToViewSpace(glm::vec2 screenUV);
    glm::vec3 ScreenUVToWorldPos(glm::vec2 screenUV);
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);

    static RefPtr<Camera> mainCamera;

    float GetProjectionRight();
    float GetProjectionTop();
    float GetNear();
    float GetFar();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
};
} // namespace Engine
