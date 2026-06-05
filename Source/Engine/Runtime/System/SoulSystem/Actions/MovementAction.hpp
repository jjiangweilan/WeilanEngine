#pragma once
#include "../SoulAction.hpp"

namespace Soul
{
class MovementAction : public Action
{
public:
    const std::vector<std::unique_ptr<ActionConstraint>>& GetConstraints() override;
    bool IsFinished() override;
    void Update() override;
    void OnStart() override;
    void OnEnd() override;

    void SetTarget(const float3& target);
    void ClearTarget();
    void SetMoveSpeed(float speed) { moveSpeed = speed; }

private:
    bool IsBlocked();
    bool IsPathValid();
    void UpdatePath();
    void ForwardTarget();

    float3 targetPosition = float3(0.0f);
    bool hasTarget = false;
    std::vector<float3> path;
    int currentWaypointIndex = 0;
    float moveSpeed = 3.0f;
    float waypointReachDistance = 0.2f;
    std::vector<std::unique_ptr<ActionConstraint>> constraints;
};
} // namespace Soul
