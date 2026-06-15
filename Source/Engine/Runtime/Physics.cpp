#include "Physics.hpp"
#include "Engine/Driver/Physics/JoltDebugRenderer.hpp"
#include "Engine/MiddleLayer/DebugOptions.hpp"
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
#include <variant>
#include <unordered_set>

namespace
{
struct PhysicsQueryContext
{
    PhysicsScene* scene = nullptr;
    JPH::PhysicsSystem* system = nullptr;
};

struct DebugRayQuery
{
    glm::vec3 origin;
    glm::vec3 end;
    PhysicsHit hit;
};

struct DebugSphereQuery
{
    glm::vec3 start;
    glm::vec3 end;
    float radius = 0.0f;
    PhysicsHit hit;
};

struct DebugBoxQuery
{
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 halfExtents = glm::vec3(0.0f);
    glm::quat rotation = glm::identity<glm::quat>();
    PhysicsHit hit;
};

struct DebugCapsuleQuery
{
    glm::vec3 start;
    glm::vec3 end;
    float halfHeight = 0.0f;
    float radius = 0.0f;
    glm::quat rotation = glm::identity<glm::quat>();
    PhysicsHit hit;
};

using DebugPhysicsQuery = std::variant<DebugRayQuery, DebugSphereQuery, DebugBoxQuery, DebugCapsuleQuery>;

std::vector<DebugPhysicsQuery>& GetDebugPhysicsQueries()
{
    static std::vector<DebugPhysicsQuery> queries;
    return queries;
}

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

glm::vec3 GetCastEnd(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    return origin + glm::normalize(direction) * maxDistance;
}

void RecordRayDebugQuery(const glm::vec3& origin, const glm::vec3& end, const PhysicsHit& hit)
{
    if (GetDebugOptions().drawPhysicsQueries)
        GetDebugPhysicsQueries().push_back(DebugRayQuery{origin, end, hit});
}

void RecordSphereDebugQuery(const glm::vec3& start, const glm::vec3& end, float radius, const PhysicsHit& hit)
{
    if (GetDebugOptions().drawPhysicsQueries)
        GetDebugPhysicsQueries().push_back(DebugSphereQuery{start, end, radius, hit});
}

void RecordBoxDebugQuery(
    const glm::vec3& start,
    const glm::vec3& end,
    const glm::vec3& halfExtents,
    const glm::quat& rotation,
    const PhysicsHit& hit
)
{
    if (GetDebugOptions().drawPhysicsQueries)
        GetDebugPhysicsQueries().push_back(DebugBoxQuery{start, end, halfExtents, rotation, hit});
}

void RecordCapsuleDebugQuery(
    const glm::vec3& start,
    const glm::vec3& end,
    float halfHeight,
    float radius,
    const glm::quat& rotation,
    const PhysicsHit& hit
)
{
    if (GetDebugOptions().drawPhysicsQueries)
        GetDebugPhysicsQueries().push_back(DebugCapsuleQuery{start, end, halfHeight, radius, rotation, hit});
}

void DrawHit(JoltDebugRenderer& renderer, const PhysicsHit& hit)
{
    if (!hit.hasHit)
        return;

    JPH::RVec3 point = ToJoltRVec3(hit.point);
    renderer.DrawWireSphere(point, 0.08f, JPH::Color::sGreen, 1);
    if (glm::length2(hit.normal) > 0.0f)
        renderer.DrawArrow(point, ToJoltRVec3(hit.point + hit.normal * 0.5f), JPH::Color::sGreen, 0.08f);
}

JPH::Color GetQueryColor(const PhysicsHit& hit)
{
    return hit.hasHit ? JPH::Color(0, 255, 255, 180) : JPH::Color(255, 160, 0, 120);
}

void DrawDebugQuery(JoltDebugRenderer& renderer, const DebugRayQuery& query)
{
    renderer.DrawArrow(ToJoltRVec3(query.origin), ToJoltRVec3(query.end), GetQueryColor(query.hit), 0.08f);
    DrawHit(renderer, query.hit);
}

void DrawDebugQuery(JoltDebugRenderer& renderer, const DebugSphereQuery& query)
{
    JPH::Color color = GetQueryColor(query.hit);
    renderer.DrawWireSphere(ToJoltRVec3(query.start), query.radius, color, 1);
    if (query.start != query.end)
    {
        renderer.DrawWireSphere(ToJoltRVec3(query.end), query.radius, JPH::Color(color, 100), 1);
        renderer.DrawArrow(ToJoltRVec3(query.start), ToJoltRVec3(query.end), color, 0.08f);
        DrawHit(renderer, query.hit);
    }
}

void DrawDebugQuery(JoltDebugRenderer& renderer, const DebugBoxQuery& query)
{
    JPH::Color color = GetQueryColor(query.hit);
    JPH::AABox box(ToJoltVec3(-query.halfExtents), ToJoltVec3(query.halfExtents));
    renderer.DrawWireBox(MakeTransform(query.start, query.rotation), box, color);
    if (query.start != query.end)
    {
        renderer.DrawWireBox(MakeTransform(query.end, query.rotation), box, JPH::Color(color, 100));
        renderer.DrawArrow(ToJoltRVec3(query.start), ToJoltRVec3(query.end), color, 0.08f);
        DrawHit(renderer, query.hit);
    }
}

void DrawDebugQuery(JoltDebugRenderer& renderer, const DebugCapsuleQuery& query)
{
    JPH::Color color = GetQueryColor(query.hit);
    renderer.DrawCapsule(MakeTransform(query.start, query.rotation), query.halfHeight, query.radius, color, JPH::DebugRenderer::ECastShadow::Off, JPH::DebugRenderer::EDrawMode::Wireframe);
    if (query.start != query.end)
    {
        renderer.DrawCapsule(MakeTransform(query.end, query.rotation), query.halfHeight, query.radius, JPH::Color(color, 100), JPH::DebugRenderer::ECastShadow::Off, JPH::DebugRenderer::EDrawMode::Wireframe);
        renderer.DrawArrow(ToJoltRVec3(query.start), ToJoltRVec3(query.end), color, 0.08f);
        DrawHit(renderer, query.hit);
    }
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
    {
        RecordRayDebugQuery(origin, origin + normalizedDirection * maxDistance, {});
        return {};
    }

    PhysicsHit hit = BuildRayHit(*context.scene, ray, result);
    RecordRayDebugQuery(origin, origin + normalizedDirection * maxDistance, hit);
    return hit;
}

PhysicsHit Physics::SphereCast(const glm::vec3& origin, float radius, const glm::vec3& direction, float maxDistance)
{
    if (radius <= 0.0f)
        return {};

    JPH::SphereShape shape(radius);
    PhysicsHit hit = CastShape(shape, MakeTransform(origin, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), direction, maxDistance);
    if (IsValidCastInput(direction, maxDistance))
        RecordSphereDebugQuery(origin, GetCastEnd(origin, direction, maxDistance), radius, hit);
    return hit;
}

PhysicsHit Physics::BoxCast(const glm::vec3& origin, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec3& direction, float maxDistance)
{
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        return {};

    JPH::BoxShape shape(ToJoltVec3(halfExtents));
    PhysicsHit hit = CastShape(shape, MakeTransform(origin, rotation), direction, maxDistance);
    if (IsValidCastInput(direction, maxDistance))
        RecordBoxDebugQuery(origin, GetCastEnd(origin, direction, maxDistance), halfExtents, rotation, hit);
    return hit;
}

PhysicsHit Physics::CapsuleCast(const glm::vec3& origin, float halfHeight, float radius, const glm::quat& rotation, const glm::vec3& direction, float maxDistance)
{
    if (halfHeight <= 0.0f || radius <= 0.0f)
        return {};

    JPH::CapsuleShape shape(halfHeight, radius);
    PhysicsHit hit = CastShape(shape, MakeTransform(origin, rotation), direction, maxDistance);
    if (IsValidCastInput(direction, maxDistance))
        RecordCapsuleDebugQuery(origin, GetCastEnd(origin, direction, maxDistance), halfHeight, radius, rotation, hit);
    return hit;
}

bool Physics::CheckSphere(const glm::vec3& center, float radius)
{
    if (radius <= 0.0f)
        return false;

    JPH::SphereShape shape(radius);
    bool hasHit = CollideShape(shape, MakeTransform(center, glm::quat(1.0f, 0.0f, 0.0f, 0.0f))).Count() > 0;
    RecordSphereDebugQuery(center, center, radius, PhysicsHit{.hasHit = hasHit});
    return hasHit;
}

bool Physics::CheckBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation)
{
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        return false;

    JPH::BoxShape shape(ToJoltVec3(halfExtents));
    bool hasHit = CollideShape(shape, MakeTransform(center, rotation)).Count() > 0;
    RecordBoxDebugQuery(center, center, halfExtents, rotation, PhysicsHit{.hasHit = hasHit});
    return hasHit;
}

PhysicsOverlapResult Physics::OverlapSphere(const glm::vec3& center, float radius)
{
    if (radius <= 0.0f)
        return {};

    JPH::SphereShape shape(radius);
    PhysicsOverlapResult result = CollideShape(shape, MakeTransform(center, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)));
    RecordSphereDebugQuery(center, center, radius, PhysicsHit{.hasHit = result.Count() > 0});
    return result;
}

PhysicsOverlapResult Physics::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation)
{
    if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        return {};

    JPH::BoxShape shape(ToJoltVec3(halfExtents));
    PhysicsOverlapResult result = CollideShape(shape, MakeTransform(center, rotation));
    RecordBoxDebugQuery(center, center, halfExtents, rotation, PhysicsHit{.hasHit = result.Count() > 0});
    return result;
}

void Physics::DebugDrawQueries()
{
    std::vector<DebugPhysicsQuery>& queries = GetDebugPhysicsQueries();
    if (!GetDebugOptions().drawPhysicsQueries)
    {
        queries.clear();
        return;
    }

    JoltDebugRenderer* renderer = JoltDebugRenderer::GetDebugRenderer().get();
    if (renderer == nullptr)
    {
        queries.clear();
        return;
    }

    for (const DebugPhysicsQuery& query : queries)
    {
        std::visit([renderer](const auto& typedQuery) { DrawDebugQuery(*renderer, typedQuery); }, query);
    }
    queries.clear();
}
