#include "Component.hpp"

namespace Engine
{
Component::Component(std::string_view name, RefPtr<GameObject> gameObject) : name(name), gameObject(gameObject.Get()) { this->gameObject = gameObject.Get(); }

Component::~Component() {}

GameObject* Component::GetGameObject() { return gameObject; }
} // namespace Engine
