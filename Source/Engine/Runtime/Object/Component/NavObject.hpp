#pragma once

#include "Component.hpp"
#include "Engine/Runtime/System/Navigation/NavSystem.hpp"

class NavObject : public Component
{
    DECLARE_COMPONENT(NavObject);

public:
    std::unique_ptr<Component> Clone(GameObject& owner) override;

private:
    void OnStart() override;
    void OnDestroy() override;
    void TransformChanged() override;

    NavObjectHandle handle{};
    bool registered = false;
};
