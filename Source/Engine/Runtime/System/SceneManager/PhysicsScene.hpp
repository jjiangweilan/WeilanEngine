#pragma once
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
// clang-format on
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/Physics/JoltDebugRenderer.hpp"
#include "PhysicsLayer.hpp"
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <unordered_set>

class PhysicsBody;
class PhysicsScene;
class Scene;

namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer Static(0);  // static object
static constexpr JPH::BroadPhaseLayer Dynamic(1); // interactable with all other objects
static constexpr JPH::BroadPhaseLayer Sprite(2);  // moving and interact with the static scene
static constexpr JPH::BroadPhaseLayer Sensor(3);  // no actual collision, only trigger events
static constexpr JPH::uint NUM_LAYERS(4);
}; // namespace BroadPhaseLayers
using BroadPhaseLayer = JPH::BroadPhaseLayer;
//
const char* MapBroadPhaseLayerToString(BroadPhaseLayer layer);

class PhysicsContactListener : public JPH::ContactListener
{
public:
    PhysicsContactListener(PhysicsScene* scene) : pScene(scene) {}

    // See: ContactListener
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult
    ) override
    {
        // spdlog::info("Contact validate callback");

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings
    ) override;

    virtual void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings
    ) override
    {
        // spdlog::info("A contact was persisted");
    }

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

private:
    PhysicsScene* pScene;
    std::unordered_map<JPH::BodyID, ObjPtr<PhysicsBody>> contactingBodies;
    std::mutex contactLock;
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
    {
        // spdlog::info("A body got activated");
    }

    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
    {
        // spdlog::info("A body went to sleep");
    }
};

class PhysicsScene
{
public:
    static constexpr float GetDeltaTime() { return PhysicsDeltaTime; };
    const int CollisionSteps = 1;

    PhysicsScene(Scene* scene);
    ~PhysicsScene();

    PhysicsScene(const PhysicsScene& other) = delete;
    PhysicsScene(PhysicsScene&& other) = delete;

    JPH::BodyInterface& GetBodyInterface() { return *bodyInterface; }

    void AddPhysicsBody(PhysicsBody& body);

    void RemovePhysicsBody(PhysicsBody& body);
    PhysicsBody* GetBodyThroughID(JPH::BodyID id)
    {
        auto iter = bodies.find(id);
        if (iter != bodies.end())
            return iter->second;

        return nullptr;
    }

    void Tick();
    void DebugDraw();

    JPH::PhysicsSystem& GetPhysicsSystem() { return physicsSystem; }

private:
    Scene* scene;
    JPH::PhysicsSystem physicsSystem;
    JPH::BodyInterface* bodyInterface;
    std::unordered_map<JPH::BodyID, PhysicsBody*> bodies;
    bool optimizeNeeded = false;
    float physicsUpdateDeltaAccumulation;
    static constexpr float PhysicsDeltaTime = 1.0f / 50;

    class DebugBodyDrawFilter : public JPH::BodyDrawFilter
    {
    public:
        DebugBodyDrawFilter() : drawRequested(32) {}
        bool ShouldDraw(const JPH::Body& inBody) const override { return drawRequested.contains(inBody.GetID()); }
        std::unordered_set<JPH::BodyID> drawRequested;

    } bodyDrawFilter;

    std::unique_ptr<JPH::ObjectLayerPairFilterTable> objectLayerPairFilter;
    std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable> broadPhaseLayerInterfaceTable;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> objectVsBroadPhaseLayerFilterTable;

    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemThreadPool job_system;
    PhysicsContactListener contact_listener;
    MyBodyActivationListener body_activation_listener;

    friend class PhysicsContactListener;
};
