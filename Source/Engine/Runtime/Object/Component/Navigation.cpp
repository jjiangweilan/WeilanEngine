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
    s->Serialize("maxSlopeDegrees", glm::degrees(pathQuery.maxSlopeRadians));
    s->Serialize("allowDiagonal", pathQuery.allowDiagonal);
    s->Serialize("smoothPath", pathQuery.smoothPath);
    s->Serialize("waypointReachDistance", steeringQuery.waypointReachDistance);
    s->Serialize("lookAheadDistance", steeringQuery.lookAheadDistance);
}

void Navigation::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    DESERIALIZE(s, navData);
    float maxSlopeDegrees = glm::degrees(pathQuery.maxSlopeRadians);
    s->Deserialize("maxSlopeDegrees", maxSlopeDegrees);
    pathQuery.maxSlopeRadians = glm::radians(glm::clamp(maxSlopeDegrees, 0.0f, 89.0f));
    s->Deserialize("allowDiagonal", pathQuery.allowDiagonal);
    s->Deserialize("smoothPath", pathQuery.smoothPath);
    s->Deserialize("waypointReachDistance", steeringQuery.waypointReachDistance);
    s->Deserialize("lookAheadDistance", steeringQuery.lookAheadDistance);
}

std::unique_ptr<Component> Navigation::Clone(GameObject& owner)
{
    std::unique_ptr<Navigation> clone = std::make_unique<Navigation>(&owner);
    clone->enabled = enabled;
    clone->SetNavData(navData);
    clone->pathQuery = pathQuery;
    clone->steeringQuery = steeringQuery;
    return clone;
}

void Navigation::SetNavData(ObjPtr<NavData> data)
{
    navData = data;
}

void Navigation::OnAwake()
{
    navSystem.SetRelativePosition(gameObject->GetPosition());
    navSystem.Init(navData);
    NavSystem::SetGlobalInstance(&navSystem);
}

void Navigation::OnDestroy()
{
    NavSystem::ClearGlobalInstance(&navSystem);
}

NavPathResult Navigation::FindPath(const float3& startWorld, const float3& endWorld)
{
    return navSystem.FindPath(startWorld, endWorld, pathQuery);
}

bool Navigation::GetSteeringTarget(const std::vector<float3>& waypoints, const float3& currentPosition, float3& outTarget) const
{
    return navSystem.GetSteeringTarget(waypoints, currentPosition, steeringQuery, outTarget);
}
