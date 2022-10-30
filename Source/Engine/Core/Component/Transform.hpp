#pragma once

#include "Component.hpp"
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine
{
    class GameObject;
    class Transform : public Component
    {
        DECLARE_COMPONENT(Transform);

        public:
            Transform();
            Transform(GameObject* gameObject);
            ~Transform() override {};

            const std::vector<RefPtr<Transform>>& GetChildren();
            RefPtr<Transform> GetParent();
            void SetParent(RefPtr<Transform> transform);
            void SetRotation(const glm::vec3& rotation);
            void SetRotation(const glm::quat& rotation);
            void SetPostion(const glm::vec3& position);
            void SetScale(const glm::vec3& scale);
            const glm::vec3& GetPosition();
            const glm::vec3& GetScale();
            const glm::vec3& GetRotation();

            const glm::quat& GetRotationQuat();

            glm::mat4 GetModelMatrix();
        private:
            void RemoveChild(RefPtr<Transform> child);

            EDITABLE(glm::quat, rotation);
            EDITABLE(glm::vec3, rotationEuler);
            EDITABLE(glm::vec3, position);
            EDITABLE(glm::vec3, scale);

            EDITABLE(RefPtr<Transform>, parent);
            EDITABLE(std::vector<RefPtr<Transform>>, children);
    };
    DUPLICABLE(Transform);
}
