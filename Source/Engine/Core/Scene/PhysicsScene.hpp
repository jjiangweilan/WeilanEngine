#pragma once
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
// clang-format on
#include "Physics/JoltDebugRenderer.hpp"
#include "PhysicsLayer.hpp"
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <spdlog/spdlog.h>
#include <unordered_set>

class PhysicsBody;

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (static_cast<PhysicsLayer>(inObject1))
        {
            case PhysicsLayer::NON_MOVING:
                return static_cast<PhysicsLayer>(inObject2) ==
                       PhysicsLayer::MOVING;        // Non moving only collides with moving
            case PhysicsLayer::MOVING: return true; // Moving collides with everything
            default: JPH_ASSERT(false); return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr JPH::uint NUM_LAYERS(2);
}; // namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[static_cast<int>(PhysicsLayer::NON_MOVING)] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[static_cast<int>(PhysicsLayer::MOVING)] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < static_cast<JPH::ObjectLayer>(PhysicsLayer::NUM_LAYERS));
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[static_cast<int>(PhysicsLayer::NUM_LAYERS)];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
            case static_cast<int>(PhysicsLayer::NON_MOVING): return inLayer2 == BroadPhaseLayers::MOVING;
            case static_cast<int>(PhysicsLayer::MOVING): return true;
            default: JPH_ASSERT(false); return false;
        }
    }
};

// An example contact listener
class MyContactListener : public JPH::ContactListener
{
public:
    // See: ContactListener
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult
    ) override
    {
        spdlog::info("Contact validate callback");

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings
    ) override
    {
        spdlog::info("A contact was added");
    }

    virtual void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings
    ) override
    {
        spdlog::info("A contact was persisted");
    }

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
    {
        spdlog::info("A contact was removed");
    }
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
    {
        spdlog::info("A body got activated");
    }

    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
    {
        spdlog::info("A body went to sleep");
    }
};

class PhysicsScene
{
public:
    PhysicsScene();
    ~PhysicsScene();

    PhysicsScene(const PhysicsScene& other) = delete;
    PhysicsScene(PhysicsScene&& other) = delete;

    JPH::BodyInterface& GetBodyInterface()
    {
        return *bodyInterface;
    }

    void AddPhysicsBody(PhysicsBody& body)
    {
        bodies.push_back(&body);
    }

    void RemovePhysicsBody(PhysicsBody& body)
    {
        auto iter = std::find(bodies.begin(), bodies.end(), &body);
        if (iter != bodies.end())
        {
            std::swap(*iter, bodies.back());
            bodies.pop_back();
        }
    }

    void Tick();
    void DebugDraw();

private:
    std::vector<PhysicsBody*> bodies;
    JPH::PhysicsSystem physicsSystem;
    JPH::BodyInterface* bodyInterface;

    JoltDebugRenderer debugRenderer;
    class DebugBodyDrawFilter : public JPH::BodyDrawFilter
    {
    public:
        DebugBodyDrawFilter() : drawRequested(32) {}
        bool ShouldDraw(const JPH::Body& inBody) const override
        {
            return drawRequested.contains(inBody.GetID());
        }
        std::unordered_set<JPH::BodyID> drawRequested;

    } bodyDrawFilter;

    BPLayerInterfaceImpl broad_phase_layer_interface;

    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;
    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemThreadPool job_system;
    MyContactListener contact_listener;
    MyBodyActivationListener body_activation_listener;
};
