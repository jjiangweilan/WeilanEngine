#include "PhysicsScene.hpp"
#include "Core/Component/PhysicsBody.hpp"

PhysicsScene::PhysicsScene()
    : temp_allocator(10 * 1024 * 1024),
      job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, JPH::thread::hardware_concurrency())
{

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

    // Now we can create the actual physics system.
    physicsSystem.Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        broad_phase_layer_interface,
        object_vs_broadphase_layer_filter,
        object_vs_object_layer_filter
    );

    physicsSystem.SetBodyActivationListener(&body_activation_listener);
    physicsSystem.SetContactListener(&contact_listener);

    bodyInterface = &physicsSystem.GetBodyInterface();

    physicsSystem.SetGravity({0, -0.98, 0});
}

PhysicsScene::~PhysicsScene() {}

void PhysicsScene::Tick()
{
    const float cDeltaTime = 1.0f / 60.0f;
    const int cCollisionSteps = 1;
    // Step the world
    physicsSystem.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);

    JPH::BodyManager::DrawSettings drawSettings;

    bodyDrawFilter.drawRequested.clear();
    for (auto b : bodies)
    {
        b->UpdateGameObject();
        if (b->debugDrawRequest)
        {
            b->debugDrawRequest = false;
            bodyDrawFilter.drawRequested.emplace(b->GetBody()->GetID());
        }
    }

    physicsSystem.DrawBodies(drawSettings, &debugRenderer, &bodyDrawFilter);
}
