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
            void SetPosition(const glm::vec3& position);
            void SetScale(const glm::vec3& scale);
            const glm::vec3& GetPosition();
            const glm::vec3& GetScale();
            const glm::vec3& GetRotation();

            const glm::quat& GetRotationQuat();

            glm::mat4 GetModelMatrix();
        private:
            void RemoveChild(RefPtr<Transform> child);

            glm::quat rotation;
            glm::vec3 rotationEuler;
            glm::vec3 position;
            glm::vec3 scale;

            RefPtr<Transform> parent;
            std::vector<RefPtr<Transform>> children;
    };
    DUPLICABLE(Transform);
}
