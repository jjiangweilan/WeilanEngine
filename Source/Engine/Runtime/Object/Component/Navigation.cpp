#include "Navigation.hpp"

#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

DEFINE_COMPONENT(Navigation, "C96A1719-7FB2-41DA-8220-38BFDA587725")

TYPE_REFLECTION_MEMBER_VARIABLES(
    Navigation,
    TYPE_REFLECTION_MEM(Navigation, navData)
);

void Navigation::DebugDraw()
{
    navSystem.SetRelativePosition(gameObject->GetPosition());
    navSystem.Visualize();
}

void Navigation::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, navData);
}

void Navigation::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    DESERIALIZE(s, navData);
    navSystem.Init(navData);
}

std::unique_ptr<Component> Navigation::Clone(GameObject& owner)
{
    std::unique_ptr<Navigation> clone = std::make_unique<Navigation>(&owner);
    clone->enabled = enabled;
    clone->SetNavData(navData);
    return clone;
}

void Navigation::SetNavData(ObjPtr<NavData> data)
{
    navData = data;
}

void Navigation::OnAwake()
{
    navSystem.Init(navData);
    NavSystem::SetGlobalInstance(&navSystem);
}

void Navigation::OnDestroy()
{
    NavSystem::ClearGlobalInstance(&navSystem);
}
