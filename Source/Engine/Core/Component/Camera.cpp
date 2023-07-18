#include "Camera.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <glm/gtc/matrix_transform.hpp>
namespace Engine
{
Camera::Camera(GameObject* gameObject) : Component("Camera", gameObject), projectionMatrix(), viewMatrix()
{
    if (mainCamera == nullptr)
    {
        mainCamera = this;
    }

    auto swapChainImage = GetGfxDriver()->GetSwapChainImageProxy();
    auto& desc = swapChainImage->GetDescription();
    SetProjectionMatrix(45.0f, desc.width / (float)desc.height, 0.01f, 1000.f);
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

float Camera::GetNear()
{
    return 0.01f;
}

float Camera::GetFar()
{
    return 1000.f;
}

glm::mat4 Camera::GetViewMatrix()
{
    return glm::inverse(gameObject->GetTransform()->GetModelMatrix());
}

void Camera::SetProjectionMatrix(float fovy, float aspect, float zNear, float zFar)
{
    projectionMatrix = glm::perspective(fovy, aspect, zNear, zFar);
    projectionMatrix[1][1] = -projectionMatrix[1][1];
}

const glm::mat4& Camera::GetProjectionMatrix()
{
    return projectionMatrix;
}

RefPtr<Camera> Camera::mainCamera = nullptr;

glm::vec3 Camera::ScreenUVToViewSpace(glm::vec2 screenUV)
{
    return glm::vec3(
        (screenUV - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{GetProjectionRight(), GetProjectionTop()},
        -GetNear()
    );
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

void Camera::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("projectionMatrix", projectionMatrix);
    s->Serialize("viewMatrix", viewMatrix);
}
void Camera::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("projectionMatrix", projectionMatrix);
    s->Deserialize("viewMatrix", viewMatrix);
}
} // namespace Engine
