#pragma once

#include "Component.hpp"

class MissingComponent : public Component
{
    DECLARE_COMPONENT(MissingComponent);

public:
    std::unique_ptr<Component> Clone(GameObject& owner) override;
};
