#include "Transform.hpp"
#include "../GameObject.hpp"
#include "Core/GameScene/GameScene.hpp"
#include <glm/gtx/matrix_decompose.hpp>
namespace Engine
{
Transform::Transform() : Component("Transform", nullptr) {}
Transform::Transform(GameObject* gameObject) : Transform()
{
    position = glm::vec3(0, 0, 0);
    scale = glm::vec3(1, 1, 1);
    rotationEuler = glm::vec3(0, 0, 0);
    rotation = glm::quat(rotationEuler);
}

const std::vector<RefPtr<Transform>>& Transform::GetChildren() { return children; }

void Transform::SetParent(RefPtr<Transform> parent)
{
    if (this->parent == parent)
        return;

    if (parent == nullptr)
    {
        GameScene* scene = gameObject->GetGameScene().Get();
        if (scene)
            scene->MoveGameObjectToRoot(gameObject);
        this->parent->RemoveChild(this);
        this->parent = nullptr;
        parent->children.push_back(this);
    }
    else if (this->parent == nullptr)
    {
        GameScene* scene = gameObject->GetGameScene().Get();
        if (scene)
            scene->RemoveGameObjectFromRoot(gameObject);
        this->parent = parent;
        parent->children.push_back(this);
    }
    else
    {
        this->parent->RemoveChild(this);
        this->parent = parent;
        parent->children.push_back(this);
    }
}

void Transform::RemoveChild(RefPtr<Transform> child)
{
    auto it = children.begin();
    while (it != children.end())
    {
        if (*it == child)
        {
            children.erase(it);
            return;
        }
        it += 1;
    }
}

RefPtr<Transform> Transform::GetParent() { return parent; }

void Transform::SetRotation(const glm::vec3& rotation)
{
    rotationEuler = rotation;
    this->rotation = glm::quat(rotation);
}

void Transform::SetRotation(const glm::quat& rotation) { this->rotation = rotation; }

void Transform::SetPosition(const glm::vec3& position) { this->position = position; }

void Transform::SetScale(const glm::vec3& scale) { this->scale = scale; }

const glm::vec3& Transform::GetPosition() { return position; }

const glm::vec3& Transform::GetScale() { return scale; }

const glm::vec3& Transform::GetRotation() { return rotationEuler; }

const glm::quat& Transform::GetRotationQuat() { return rotation; }

void Transform::SetModelMatrix(const glm::mat4& model)
{
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(model, scale, rotation, position, skew, perspective);
    rotationEuler = glm::eulerAngles(rotation);
}

glm::mat4 Transform::GetModelMatrix()
{
    glm::mat4 rst = glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);

    return rst;
}

void Transform::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("rotation", rotation);
    s->Deserialize("rotationEuler", rotationEuler);
    s->Deserialize("position", position);
    s->Deserialize("scale", scale);
    s->Deserialize("parent", parent);
    s->Deserialize("children", children);
}

void Transform::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("rotation", rotation);
    s->Serialize("rotationEuler", rotationEuler);
    s->Serialize("position", position);
    s->Serialize("scale", scale);
    s->Serialize("parent", parent);
    s->Serialize("children", children);
}
} // namespace Engine
