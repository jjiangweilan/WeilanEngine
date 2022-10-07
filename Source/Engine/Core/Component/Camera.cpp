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

    Camera::Camera() : Component("Camera", nullptr), projectionMatrix(), viewMatrix(){
        if (mainCamera == nullptr)
        {
            mainCamera = this;
        }
    };

    glm::mat4 Camera::GetViewMatrix()
    {
        return glm::inverse(gameObject->GetTransform()->GetModelMatrix());
    }

    const glm::mat4& Camera::GetProjectionMatrix()
    {
        return projectionMatrix;
    }

    RefPtr<Camera> Camera::mainCamera = nullptr;
}
