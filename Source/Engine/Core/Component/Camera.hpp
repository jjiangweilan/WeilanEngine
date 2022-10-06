#pragma once

#include "Component.hpp"
#include <glm/glm.hpp>
namespace Engine
{
    class Camera : public Component
    {
        DECLARE_COMPONENT(Camera);

        public:
            Camera();
            Camera(GameObject* gameObject);
            ~Camera() override{};
            glm::mat4 GetViewMatrix();
            const glm::mat4& GetProjectionMatrix();

            static RefPtr<Camera> mainCamera;

            float GetProjectionRight();
            float GetProjectionTop();
            float GetNear();
            float GetFar();
        
        private:
            EDITABLE(glm::mat4, projectionMatrix);
            EDITABLE(glm::mat4, viewMatrix);
    };
}
