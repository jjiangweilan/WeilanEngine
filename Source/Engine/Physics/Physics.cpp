#include "Physics.hpp"
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
#include <sstream>
#include <stdarg.h>

void Physics::Init()
{

    // Next we can create a rigid body to serve as the floor, we make a large box
    // Create the settings for the collision volume (the shape).
    // Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
    BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));

    // Create the shape
    ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    ShapeRefC floor_shape = floor_shape_result.Get(
    ); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

    // Create the settings for the body itself. Note that here you can also set other properties like the restitution /
    // friction.
    BodyCreationSettings floor_settings(
        floor_shape,
        RVec3(0.0_r, -1.0_r, 0.0_r),
        Quat::sIdentity(),
        EMotionType::Static,
        Layers::NON_MOVING
    );

    // Create the actual rigid body
    Body* floor =
        body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

    // Add it to the world
    body_interface.AddBody(floor->GetID(), EActivation::DontActivate);

    // Now create a dynamic body to bounce on the floor
    // Note that this uses the shorthand version of creating and adding a body to the world
    BodyCreationSettings sphere_settings(
        new SphereShape(0.5f),
        RVec3(0.0_r, 2.0_r, 0.0_r),
        Quat::sIdentity(),
        EMotionType::Dynamic,
        Layers::MOVING
    );
    BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to
    // the physics system)
    body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));

    // We simulate the physics world in discrete time steps. 60 Hz is a good rate to update the physics system.
    const float cDeltaTime = 1.0f / 60.0f;

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision
    // detection performance (it's pointless here because we only have 2 bodies). You should definitely not call this
    // every frame or when e.g. streaming in a new level section as it is an expensive operation. Instead insert all new
    // objects in batches instead of 1 at a time to keep the broad phase efficient.
    physics_system.OptimizeBroadPhase();

    // Now we're ready to simulate the body, keep simulating until it goes to sleep
    uint step = 0;
    while (body_interface.IsActive(sphere_id))
    {
        // Next step
        ++step;

        // Output current position and velocity of the sphere
        RVec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
        Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);

        std::stringstream ss;
        ss << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", "
           << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", "
           << velocity.GetZ() << ")" << endl;
        spdlog::info(ss.str());

        // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep
        // the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
        const int cCollisionSteps = 1;

        // Step the world
        physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
    }

    // Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added
    // at any time.
    body_interface.RemoveBody(sphere_id);

    // Destroy the sphere. After this the sphere ID is no longer valid.
    body_interface.DestroyBody(sphere_id);

    // Remove and destroy the floor
    body_interface.RemoveBody(floor->GetID());
    body_interface.DestroyBody(floor->GetID());

    // Unregisters all types with the factory and cleans up the default material
    UnregisterTypes();

    // Destroy the factory
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
}
void Physics::Destroy(){};
