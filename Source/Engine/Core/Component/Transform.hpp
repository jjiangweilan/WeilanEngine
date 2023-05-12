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
public:
    Transform();
    Transform(GameObject* gameObject);
    ~Transform() override{};

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
    void SetModelMatrix(const glm::mat4& model);

private:
    void RemoveChild(RefPtr<Transform> child);

    glm::quat rotation;
    glm::vec3 rotationEuler;
    glm::vec3 position;
    glm::vec3 scale;

    RefPtr<Transform> parent;
    std::vector<RefPtr<Transform>> children;

    friend struct SerializableField<Transform>;
};

template <>
struct SerializableField<Transform>
{
    static void Serialize(Transform* v, Serializer* s)
    {
        SerializableField<Component>::Serialize(v, s);
        s->Serialize(v->rotation);
        s->Serialize(v->rotationEuler);
        s->Serialize(v->position);
        s->Serialize(v->scale);
        s->Serialize(v->parent);
        s->Serialize(v->children);
    }

    static void Deserialize(Transform* v, Serializer* s)
    {
        SerializableField<Component>::Deserialize(v, s);
        s->Deserialize(v->rotation);
        s->Deserialize(v->rotationEuler);
        s->Deserialize(v->position);
        s->Deserialize(v->scale);
        s->Deserialize(v->parent);
        s->Deserialize(v->children);
    }
};

} // namespace Engine
