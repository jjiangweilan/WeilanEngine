#pragma once

#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include <memory>
#include <vector>

namespace Soul
{

class ActionConstraint
{
public:
    virtual ~ActionConstraint() = default;
    virtual bool IsSatisfied() = 0;
};

class Action
{
public:
    virtual ~Action() = default;

    void SetPriority(float priority);
    float GetPriority() const;
    void SetGameObject(GameObject* gameObject);

    virtual const std::vector<std::unique_ptr<ActionConstraint>>& GetConstraints() = 0;
    virtual bool IsFinished() = 0;
    virtual void Update() = 0;
    virtual void OnStart() = 0;
    virtual void OnEnd() = 0;

protected:
    float priority = 100;
    GameObject* gameObject = nullptr;
};
} // namespace Soul
