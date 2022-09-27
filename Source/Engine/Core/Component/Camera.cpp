#include "Camera.hpp"
#include "Core/Globals.hpp"
#include "Core/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
namespace Engine
{
    Camera::Camera(GameObject* gameObject) : Component("Camera", gameObject), projectionMatrix(), viewMatrix()
    {
        if (Globals::GetMainCamera() == nullptr)
        {
            Globals::SetMainCamera(this);
        }

        projectionMatrix = glm::perspective(45.f, 1920.0f / 1080.f, 0.01f, 1000.f);
        projectionMatrix[1][1] = -projectionMatrix[1][1];
    }

    Camera::Camera() : Component("Camera", nullptr), projectionMatrix(), viewMatrix(){
        if (Globals::GetMainCamera() == nullptr)
        {
            Globals::SetMainCamera(this);
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
}
