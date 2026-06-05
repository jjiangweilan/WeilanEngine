#include "MovementAction.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Runtime/System/Navigation/NavSystem.hpp"

#include <glm/geometric.hpp>

namespace Soul
{
const std::vector<std::unique_ptr<ActionConstraint>>& MovementAction::GetConstraints()
{
    return constraints;
}

bool MovementAction::IsFinished()
{
    return hasTarget && !path.empty() && currentWaypointIndex >= static_cast<int>(path.size());
}

void MovementAction::Update()
{
    if (!gameObject || !hasTarget)
        return;

    if (IsBlocked())
    {
        UpdatePath();
    }

    if (IsPathValid())
    {
        ForwardTarget();
    }
}

void MovementAction::OnStart()
{
    path.clear();
    currentWaypointIndex = 0;
    UpdatePath();
}

void MovementAction::OnEnd()
{
    path.clear();
    currentWaypointIndex = 0;
}

void MovementAction::SetTarget(const float3& target)
{
    targetPosition = target;
    hasTarget = true;
}

void MovementAction::ClearTarget()
{
    hasTarget = false;
    path.clear();
    currentWaypointIndex = 0;
}

bool MovementAction::IsBlocked()
{
    return false;
}

bool MovementAction::IsPathValid()
{
    return !path.empty() && currentWaypointIndex < static_cast<int>(path.size());
}

void MovementAction::UpdatePath()
{
    path.clear();
    currentWaypointIndex = 0;

    if (!gameObject || !hasTarget)
        return;

    auto navSystem = NavSystem::GetGlobalInstance();
    if (!navSystem)
        return;

    NavPathResult result = navSystem->FindPath(gameObject->GetPosition(), targetPosition);
    if (result.success)
    {
        path = std::move(result.waypoints);
        currentWaypointIndex = 0;
    }
}

void MovementAction::ForwardTarget()
{
    while (currentWaypointIndex < static_cast<int>(path.size()))
    {
        const float distance = glm::length(path[currentWaypointIndex] - gameObject->GetPosition());
        if (distance > waypointReachDistance)
            break;

        ++currentWaypointIndex;
    }

    if (currentWaypointIndex >= static_cast<int>(path.size()))
    {
        gameObject->SetPosition(path.back());
        return;
    }

    const float3 currentPosition = gameObject->GetPosition();
    const float3 targetWaypoint = path[currentWaypointIndex];
    const float3 toTarget = targetWaypoint - currentPosition;
    const float distance = glm::length(toTarget);
    if (distance <= 0.0001f)
        return;

    const float step = moveSpeed * Time::DeltaTime();
    if (step >= distance)
    {
        gameObject->SetPosition(targetWaypoint);
        ++currentWaypointIndex;
    }
    else
    {
        gameObject->SetPosition(currentPosition + glm::normalize(toTarget) * step);
    }
}

} // namespace Soul
