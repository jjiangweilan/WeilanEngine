#include "NavObject.hpp"

#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

DEFINE_COMPONENT(NavObject, "339E3B63-F3F1-471B-AEFD-34281F4BA4A7")

void NavObject::OnStart()
{
    if (registered)
        return;

    NavSystem* navSystem = NavSystem::GetGlobalInstance();
    if (navSystem == nullptr)
        return;

    handle = navSystem->AddNavObject(gameObject);
    registered = true;
}

void NavObject::OnDestroy()
{
    if (!registered)
        return;

    NavSystem* navSystem = NavSystem::GetGlobalInstance();
    if (navSystem != nullptr)
    {
        navSystem->RemoveNavObject(handle);
    }
    registered = false;
}

void NavObject::TransformChanged()
{
    if (!registered)
        return;

    NavSystem* navSystem = NavSystem::GetGlobalInstance();
    if (navSystem != nullptr)
    {
        navSystem->UpdateRuntimeNavObject(handle);
    }
}

std::unique_ptr<Component> NavObject::Clone(GameObject& owner)
{
    std::unique_ptr<NavObject> clone = std::make_unique<NavObject>(&owner);
    clone->enabled = enabled;
    return clone;
}
