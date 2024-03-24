#pragma once
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
// clang-format on
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

class PhysicsBody;
using namespace JPH;
using namespace JPH::literals;

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
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
static constexpr BroadPhaseLayer NON_MOVING(0);
static constexpr BroadPhaseLayer MOVING(1);
static constexpr uint NUM_LAYERS(2);
}; // namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[static_cast<int>(PhysicsLayer::NON_MOVING)] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[static_cast<int>(PhysicsLayer::MOVING)] = BroadPhaseLayers::MOVING;
    }

    virtual uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < PhysicsLayer::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
            case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    BroadPhaseLayer mObjectToBroadPhase[static_cast<int>(PhysicsLayer::NUM_LAYERS)];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
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
class MyContactListener : public ContactListener
{
public:
    // See: ContactListener
    virtual ValidateResult OnContactValidate(
        const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset, const CollideShapeResult& inCollisionResult
    ) override
    {
        spdlog::info("Contact validate callback");

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(
        const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings
    ) override
    {
        spdlog::info("A contact was added");
    }

    virtual void OnContactPersisted(
        const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings
    ) override
    {
        spdlog::info("A contact was persisted");
    }

    virtual void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
    {
        spdlog::info("A contact was removed");
    }
};

// An example activation listener
class MyBodyActivationListener : public BodyActivationListener
{
public:
    virtual void OnBodyActivated(const BodyID& inBodyID, uint64 inBodyUserData) override
    {
        spdlog::info("A body got activated");
    }

    virtual void OnBodyDeactivated(const BodyID& inBodyID, uint64 inBodyUserData) override
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

private:
    std::vector<PhysicsBody*> bodies;
    JPH::PhysicsSystem physicsSystem;
    JPH::BodyInterface* bodyInterface;

    BPLayerInterfaceImpl broad_phase_layer_interface;

    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;
    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemThreadPool job_system;
    MyContactListener contact_listener;
    MyBodyActivationListener body_activation_listener;
};
