#pragma once

#include "Component.hpp"
#include "Engine/Runtime/System/Navigation/NavSystem.hpp"

class Navigation : public Component
{
    DECLARE_COMPONENT(Navigation);

public:
    void OnAwake() override;
    void OnDestroy() override;
    void DebugDraw() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;

    void SetNavData(ObjPtr<NavData> data);
    ObjPtr<NavData> GetNavData() const { return navData; }
    NavSystem& GetNavSystem() { return navSystem; }
    const NavSystem& GetNavSystem() const { return navSystem; }

    NavPathQuery& GetPathQuery() { return pathQuery; }
    const NavPathQuery& GetPathQuery() const { return pathQuery; }
    NavSteeringQuery& GetSteeringQuery() { return steeringQuery; }
    const NavSteeringQuery& GetSteeringQuery() const { return steeringQuery; }

    NavPathResult FindPath(const float3& startWorld, const float3& endWorld);
    bool GetSteeringTarget(const std::vector<float3>& waypoints, const float3& currentPosition, float3& outTarget) const;

private:
    ObjPtr<NavData> navData;
    NavSystem navSystem;
    NavPathQuery pathQuery;
    NavSteeringQuery steeringQuery;
};
