#pragma once
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include <vector>

struct [[LuaClass]] PhysicsHit
{
    [[LuaProp]] bool hasHit = false;
    [[LuaProp]] float3 point = {};
    [[LuaProp]] float3 normal = {};
    [[LuaProp]] float distance = 0.0f;

    [[LuaFn]] PhysicsBody* GetBody() const { return body; }

    PhysicsBody* body = nullptr;
};

class [[LuaClass]] PhysicsOverlapResult
{
public:
    [[LuaFn]] int Count() const;
    [[LuaFn]] PhysicsBody* GetBody(int index) const;
    void AddBody(PhysicsBody* body) { bodies.push_back(body); }

private:
    std::vector<PhysicsBody*> bodies;
    friend class Physics;
};

class [[LuaClass]] Physics
{
public:
    [[LuaFn]] static PhysicsHit RayCast(const float3& origin, const float3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit SphereCast(const float3& origin, float radius, const float3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit BoxCast(const float3& origin, const float3& halfExtents, const glm::quat& rotation, const float3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit CapsuleCast(const float3& origin, float halfHeight, float radius, const glm::quat& rotation, const float3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit RayCastFiltered(const float3& origin, const float3& direction, float maxDistance, int layerMask);
    [[LuaFn]] static PhysicsHit SphereCastFiltered(const float3& origin, float radius, const float3& direction, float maxDistance, int layerMask);
    [[LuaFn]] static PhysicsHit BoxCastFiltered(const float3& origin, const float3& halfExtents, const glm::quat& rotation, const float3& direction, float maxDistance, int layerMask);
    [[LuaFn]] static PhysicsHit CapsuleCastFiltered(const float3& origin, float halfHeight, float radius, const glm::quat& rotation, const float3& direction, float maxDistance, int layerMask);

    [[LuaFn]] static bool CheckSphere(const float3& center, float radius);
    [[LuaFn]] static bool CheckBox(const float3& center, const float3& halfExtents, const glm::quat& rotation);
    [[LuaFn]] static bool CheckSphereFiltered(const float3& center, float radius, int layerMask);
    [[LuaFn]] static bool CheckBoxFiltered(const float3& center, const float3& halfExtents, const glm::quat& rotation, int layerMask);

    [[LuaFn]] static PhysicsOverlapResult OverlapSphere(const float3& center, float radius);
    [[LuaFn]] static PhysicsOverlapResult OverlapBox(const float3& center, const float3& halfExtents, const glm::quat& rotation);
    [[LuaFn]] static PhysicsOverlapResult OverlapSphereFiltered(const float3& center, float radius, int layerMask);
    [[LuaFn]] static PhysicsOverlapResult OverlapBoxFiltered(const float3& center, const float3& halfExtents, const glm::quat& rotation, int layerMask);

    static void DebugDrawQueries();
};
