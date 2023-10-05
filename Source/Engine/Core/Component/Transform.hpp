#pragma once

#include "Component.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace Engine
{
class GameObject;

class Transform : public Component
{
    DECLARE_OBJECT();

public:
    Transform();
    Transform(GameObject* gameObject);
    ~Transform() override{};

    const std::vector<Transform*>& GetChildren();
    Transform* GetParent();
    void SetParent(Transform* transform);
    void SetRotation(const glm::vec3& rotation);
    void SetRotation(const glm::quat& rotation);
    void SetPosition(const glm::vec3& position);
    void SetScale(const glm::vec3& scale);
    const glm::vec3& GetPosition();
    const glm::vec3& GetScale();
    const glm::vec3& GetRotation();
    glm::vec3 GetForward();

    const glm::quat& GetRotationQuat();

    glm::mat4 GetModelMatrix();
    void SetModelMatrix(const glm::mat4& model);

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    void RemoveChild(Transform* child);

    glm::quat rotation;
    glm::vec3 rotationEuler;
    glm::vec3 position;
    glm::vec3 scale;

    Transform* parent;
    std::vector<Transform*> children;
};

} // namespace Engine
