#include "TransformNode.hpp"
TransformNode& TransformNode::SetParent(TransformNode* newParent, bool keepWorldSpacePostion)
{
    if (this->parent == newParent || HasFlag(flags, TransformFlag::DontChangeHierarchy))
    {
        return;
    }

    if (newParent == nullptr)
    {
        Scene* scene = GetScene();
        if (scene)
            scene->MoveGameObjectToRoot(this);

        this->parent->RemoveChild(this);
    }

    if (this->parent == nullptr)
    {
        Scene* scene = GetScene();
        if (scene)
            scene->RemoveGameObjectFromRoot(this);
    }
    else
    {
        this->parent->RemoveChild(this);
    }

    // fix local transforms
    if (keepWorldSpacePostion)
    {
        glm::mat4 parentWorld = glm::mat4(1);
        if (newParent != nullptr)
        {
            parentWorld = newParent->GetWorldMatrix();
        }
        glm::mat4 currentWorld = GetWorldMatrix();

        glm::mat4 local = glm::inverse(parentWorld) * currentWorld;

        glm::vec3 newPosition, newScale;
        glm::quat newRotation;
        Math::DecomposeMatrix(local, newPosition, newScale, newRotation);
        SetLocalPosition(newPosition);
        SetEulerAngles(glm::eulerAngles(newRotation));
        SetLocalScale(newScale);
    }

    this->parent = newParent;
    if (newParent)
        newParent->children.push_back(this);
}
void TransformNode::RemoveChild(TransformNode* node)
{
    auto it = children.begin();
    while (it != children.end())
    {
        if ((*it).get() == node)
        {
            children.erase(it);
            return;
        }
        it += 1;
    }
}
std::span<TransformNode> GetChildren();
