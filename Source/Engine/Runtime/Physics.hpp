#pragma once
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include <vector>

struct [[LuaClass]] PhysicsHit
{
    [[LuaProp]] bool hasHit = false;
    [[LuaProp]] glm::vec3 point = {};
    [[LuaProp]] glm::vec3 normal = {};
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
    [[LuaFn]] static PhysicsHit RayCast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit SphereCast(const glm::vec3& origin, float radius, const glm::vec3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit BoxCast(const glm::vec3& origin, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec3& direction, float maxDistance);
    [[LuaFn]] static PhysicsHit CapsuleCast(const glm::vec3& origin, float halfHeight, float radius, const glm::quat& rotation, const glm::vec3& direction, float maxDistance);

    [[LuaFn]] static bool CheckSphere(const glm::vec3& center, float radius);
    [[LuaFn]] static bool CheckBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation);

    [[LuaFn]] static PhysicsOverlapResult OverlapSphere(const glm::vec3& center, float radius);
    [[LuaFn]] static PhysicsOverlapResult OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation);
};
