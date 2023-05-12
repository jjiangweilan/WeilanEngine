#include "Camera.hpp"
#include "Core/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
namespace Engine
{
Camera::Camera(GameObject* gameObject) : Component("Camera", gameObject), projectionMatrix(), viewMatrix()
{
    if (mainCamera == nullptr)
    {
        mainCamera = this;
    }

    projectionMatrix = glm::perspective(45.f, 1920.0f / 1080.f, 0.01f, 1000.f);
    projectionMatrix[1][1] = -projectionMatrix[1][1];
}

Camera::Camera() : Component("Camera", nullptr), projectionMatrix(), viewMatrix()
{
    if (mainCamera == nullptr)
    {
        mainCamera = this;
    }
};

float Camera::GetProjectionRight()
{
    return 0.01f / projectionMatrix[0][0]; // projection matrix is hard coded
}

float Camera::GetProjectionTop()
{
    return 0.01f / projectionMatrix[1][1]; // projection matrix is hard coded
}

float Camera::GetNear() { return 0.01f; }

float Camera::GetFar() { return 1000.f; }

glm::mat4 Camera::GetViewMatrix() { return glm::inverse(gameObject->GetTransform()->GetModelMatrix()); }

const glm::mat4& Camera::GetProjectionMatrix() { return projectionMatrix; }

RefPtr<Camera> Camera::mainCamera = nullptr;

glm::vec3 Camera::ScreenUVToViewSpace(glm::vec2 screenUV)
{
    return glm::vec3((screenUV - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{GetProjectionRight(), GetProjectionTop()},
                     -GetNear());
}

glm::vec3 Camera::ScreenUVToWorldPos(glm::vec2 screenUV)
{
    glm::mat4 camModelMatrix = GetGameObject()->GetTransform()->GetModelMatrix();
    return camModelMatrix * glm::vec4(ScreenUVToViewSpace(screenUV), 1.0);
}

Ray Camera::ScreenUVToWorldSpaceRay(glm::vec2 screenUV)
{
    Ray ray;
    ray.origin = GetGameObject()->GetTransform()->GetPosition();
    glm::mat4 camModelMatrix = GetGameObject()->GetTransform()->GetModelMatrix();
    glm::vec3 clickInWS = camModelMatrix * glm::vec4(ScreenUVToViewSpace(screenUV), 1.0);
    ray.direction = clickInWS - ray.origin;
    return ray;
}
} // namespace Engine
