#include "SoulAction.hpp"

namespace Soul
{
void Action::SetPriority(float priority)
{
    this->priority = priority;
}

float Action::GetPriority() const
{
    return priority;
}

void Action::SetGameObject(GameObject* gameObject)
{
    this->gameObject = gameObject;
}
} // namespace Soul
