#include "Physics.hpp"
#include "Engine/Runtime/System/SceneManager/PhysicsScene.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/SceneManager/SceneManager.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <unordered_set>

namespace
{
struct PhysicsQueryContext
{
    PhysicsScene* scene = nullptr;
    JPH::PhysicsSystem* system = nullptr;
};

PhysicsQueryContext GetPhysicsQueryContext()
{
    Scene* activeScene = SceneManager::GetActiveScene();
    if (activeScene == nullptr)
        return {};

    PhysicsScene& physicsScene = activeScene->GetPhysicsScene();
    return {&physicsScene, &physicsScene.GetPhysicsSystem()};
}

JPH::Vec3 ToJoltVec3(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

JPH::RVec3 ToJoltRVec3(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

JPH::Quat ToJoltQuat(const glm::quat& q)
{
    return {q.x, q.y, q.z, q.w};
}

template <class T>
glm::vec3 ToGlmVec3(const T& v)
{
    return {static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())};
}

bool IsValidCastInput(const glm::vec3& direction, float maxDistance)
{
    return maxDistance > 0.0f && glm::length2(direction) > 0.0f;
}

PhysicsBody* GetBody(PhysicsScene& scene, JPH::BodyID bodyID)
{
    if (bodyID.IsInvalid())
        return nullptr;

    return scene.GetBodyThroughID(bodyID);
}

PhysicsHit BuildRayHit(PhysicsScene& scene, const JPH::RRayCast& ray, const JPH::RayCastResult& result)
{
    PhysicsHit hit;
    hit.hasHit = true;
    hit.point = ToGlmVec3(ray.GetPointOnRay(result.mFraction));
    hit.distance = glm::length(ToGlmVec3(ray.mDirection)) * result.mFraction;
    hit.body = GetBody(scene, result.mBodyID);

    if (hit.body && hit.body->GetBody())
        hit.normal = ToGlmVec3(hit.body->GetBody()->GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));

    return hit;
}

PhysicsHit BuildShapeCastHit(PhysicsScene& scene, const JPH::RShapeCast& shapeCast, const JPH::ShapeCastResult& result)
{
    PhysicsHit hit;
    hit.hasHit = true;
    hit.point = ToGlmVec3(result.mContactPointOn2);
    hit.distance = glm::length(ToGlmVec3(shapeCast.mDirection)) * result.mFraction;
    hit.body = GetBody(scene, result.mBodyID2);

    if (result.mPenetrationAxis.LengthSq() > 0.0f)
        hit.normal = ToGlmVec3(-result.mPenetrationAxis.Normalized());

    return hit;
}

PhysicsHit CastShape(JPH::Shape& shape, const JPH::RMat44& transform, const glm::vec3& direction, float maxDistance)
{
    if (!IsValidCastInput(direction, maxDistance))
        return {};

    PhysicsQueryContext context = GetPhysicsQueryContext();
    if (context.system == nullptr || context.scene == nullptr)
        return {};

    glm::vec3 normalizedDirection = glm::normalize(direction);
    JPH::RShapeCast shapeCast(
        &shape,
        JPH::Vec3::sReplicate(1.0f),
        transform,
        ToJoltVec3(normalizedDirection * maxDistance)
    );

    JPH::ShapeCastSettings settings;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    context.system->GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(), collector);
    if (!collector.HadHit())
        return {};

    return BuildShapeCastHit(*context.scene, shapeCast, collector.mHit);
}

PhysicsOverlapResult CollideShape(JPH::Shape& shape, const JPH::RMat44& transform)
{
    PhysicsOverlapResult result;

    PhysicsQueryContext context = GetPhysicsQueryContext();
    if (context.system == nullptr || context.scene == nullptr)
        return result;

    JPH::CollideShapeSettings settings;
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    context.system->GetNarrowPhaseQuery().CollideShape(
        &shape,
        JPH::Vec3::sReplicate(1.0f),
        transform,
        settings,
        JPH::RVec3::sZero(),
        collector
    );

    std::unordered_set<JPH::BodyID> addedBodies;
    for (const JPH::CollideShapeResult& hit : collector.mHits)
    {
        if (addedBodies.contains(hit.mBodyID2))
            continue;

        if (PhysicsBody* body = GetBody(*context.scene, hit.mBodyID2))
        {
            result.AddBody(body);
            addedBodies.emplace(hit.mBodyID2);
        }
    }

    return result;
}

JPH::RMat44 MakeTransform(const glm::vec3& position, const glm::quat& rotation)
{
    return JPH::RMat44::sRotationTranslation(ToJoltQuat(rotation), ToJoltRVec3(position));
}
} // namespace

int PhysicsOverlapResult::Count() const
{
    return static_cast<int>(bodies.size());
}

PhysicsBody* PhysicsOverlapResult::GetBody(int index) const
{
    if (index < 0 || index >= static_cast<int>(bodies.size()))
        return nullptr;

    return bodies[index];
}

PhysicsHit Physics::RayCast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    if (!IsValidCastInput(direction, maxDistance))
        return {};

    PhysicsQueryContext context = GetPhysicsQueryContext();
    if (context.system == nullptr || context.scene == nullptr)
        return {};

    glm::vec3 normalizedDirection = glm::normalize(direction);
    JPH::RRayCast ray(ToJoltRVec3(origin), ToJoltVec3(normalizedDirection * maxDistance));
    JPH::RayCastResult result;
    if (!context.system->GetNarrowPhaseQuery().CastRay(ray, result))
        return {};

    return BuildRayHit(*context.scene, ray, result);
}

PhysicsHit Physics::SphereCast(const glm::vec3& origin, float radius, const glm::vec3& direction, float maxDistance)
{
    if (radius <= 0.0f)
        return {};

    JPH::SphereShape shape(radius);
    return CastShape(shape, MakeTransform(origin, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), direction, maxDistance);
}

PhysicsHit Physics::BoxCast(const glm::vec3& origin, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec3& direction, float maxDistance)
{
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        return {};

    JPH::BoxShape shape(ToJoltVec3(halfExtents));
    return CastShape(shape, MakeTransform(origin, rotation), direction, maxDistance);
}

PhysicsHit Physics::CapsuleCast(const glm::vec3& origin, float halfHeight, float radius, const glm::quat& rotation, const glm::vec3& direction, float maxDistance)
{
    if (halfHeight <= 0.0f || radius <= 0.0f)
        return {};

    JPH::CapsuleShape shape(halfHeight, radius);
    return CastShape(shape, MakeTransform(origin, rotation), direction, maxDistance);
}

bool Physics::CheckSphere(const glm::vec3& center, float radius)
{
    if (radius <= 0.0f)
        return false;

    JPH::SphereShape shape(radius);
    return CollideShape(shape, MakeTransform(center, glm::quat(1.0f, 0.0f, 0.0f, 0.0f))).Count() > 0;
}

bool Physics::CheckBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation)
{
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        return false;

    JPH::BoxShape shape(ToJoltVec3(halfExtents));
    return CollideShape(shape, MakeTransform(center, rotation)).Count() > 0;
}

PhysicsOverlapResult Physics::OverlapSphere(const glm::vec3& center, float radius)
{
    if (radius <= 0.0f)
        return {};

    JPH::SphereShape shape(radius);
    return CollideShape(shape, MakeTransform(center, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)));
}

PhysicsOverlapResult Physics::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation)
{
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        return {};

    JPH::BoxShape shape(ToJoltVec3(halfExtents));
    return CollideShape(shape, MakeTransform(center, rotation));
}
