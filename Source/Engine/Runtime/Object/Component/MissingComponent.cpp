#include "MissingComponent.hpp"

DEFINE_COMPONENT(MissingComponent, "BD570BBE-90B3-4CD7-9A28-7EF1BBFAEB40")

std::unique_ptr<Component> MissingComponent::Clone(GameObject& owner)
{
    auto clone = std::make_unique<MissingComponent>(&owner);
    clone->enabled = enabled;
    return clone;
}
