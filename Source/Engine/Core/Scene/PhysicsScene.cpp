#include "PhysicsScene.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <spdlog/spdlog.h>
#include <stdarg.h>

using namespace JPH;
using namespace JPH::literals;
static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    // Print to the TTY
    spdlog::info(buffer);
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
    spdlog::error("{}: {}:({}) ({})", inFile, inLine, inExpression, inMessage == nullptr ? inMessage : "");

    // Breakpoint
    return true;
};

#endif // JPH_ENABLE_ASSERTS

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics
// simulation but only if you do collision testing).
namespace Layers
{
static constexpr ObjectLayer NON_MOVING = 0;
static constexpr ObjectLayer MOVING = 1;
static constexpr ObjectLayer NUM_LAYERS = 2;
}; // namespace Layers

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
            case Layers::NON_MOVING: return inObject2 == Layers::MOVING; // Non moving only collides with moving
            case Layers::MOVING: return true;                            // Moving collides with everything
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
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
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
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
            case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING: return true;
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
PhysicsScene::PhysicsScene()
{
    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if
    // you want (see Memory.h). This needs to be done before any other Jolt function is called.
    JPH::RegisterDefaultAllocator();

    // Install trace and assert callbacks
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    // Create a factory, this class is responsible for creating instances of classes based on their name or hash and
    // is mainly used for deserialization of saved data. It is not directly used in this example but still required.
    JPH::Factory::sInstance = new Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch
    // class. If you have your own custom shape types you probably need to register their handlers with the
    // CollisionDispatch before calling this function. If you implement your own default material
    // (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create
    // one for you.
    JPH::RegisterTypes();

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
    // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
    // malloc / free.
    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

    // We need a job system that will execute physics jobs on multiple threads. Typically
    // you would implement the JobSystem interface yourself and let Jolt Physics run on top
    // of your own job scheduler. JobSystemThreadPool is an example implementation.
    JPH::JobSystemThreadPool job_system(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        JPH::thread::hardware_concurrency() - 1
    );

    // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll
    // get an error. Note: This value is low because this is a simple test. For a real project use something in the
    // order of 65536.
    const JPH::uint cMaxBodies = 1024;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for
    // the default settings.
    const JPH::uint cNumBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make
    // this buffer too small the queue will fill up and the broad phase jobs will start to do narrow phase work.
    // This is slightly less efficient. Note: This value is low because this is a simple test. For a real project
    // use something in the order of 65536.
    const JPH::uint cMaxBodyPairs = 1024;

    // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are
    // detected than this number then these contacts will be ignored and bodies will start interpenetrating / fall
    // through the world. Note: This value is low because this is a simple test. For a real project use something in
    // the order of 10240.
    const JPH::uint cMaxContactConstraints = 1024;

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay
    // alive!
    BPLayerInterfaceImpl broad_phase_layer_interface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay
    // alive!
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay
    // alive!
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;

    // Now we can create the actual physics system.
    JPH::PhysicsSystem physics_system;
    physics_system.Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        broad_phase_layer_interface,
        object_vs_broadphase_layer_filter,
        object_vs_object_layer_filter
    );

    // A body activation listener gets notified when bodies activate and go to sleep
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    MyBodyActivationListener body_activation_listener;
    physics_system.SetBodyActivationListener(&body_activation_listener);

    // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    MyContactListener contact_listener;
    physics_system.SetContactListener(&contact_listener);

    // The main way to interact with the bodies in the physics system is through the body interface. There is a
    // locking and a non-locking variant of this. We're going to use the locking version (even though we're not
    // planning to access bodies from multiple threads)
    BodyInterface& body_interface = physics_system.GetBodyInterface();
}
