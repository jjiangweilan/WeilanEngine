#include "PhysicsScene.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Time.hpp"

PhysicsScene::PhysicsScene(Scene* scene)
    : scene(scene), physicsUpdateDeltaAccumulation(0.0f), temp_allocator(10 * 1024 * 1024),
      job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, JPH::thread::hardware_concurrency()),
      contact_listener(this)
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

    physicsSystem.SetGravity({0, -9.8, 0});
}

PhysicsScene::~PhysicsScene() {}

void PhysicsScene::Tick()
{
    if (optimizeNeeded)
    {
        physicsSystem.OptimizeBroadPhase();
        optimizeNeeded = false;
    }

    physicsUpdateDeltaAccumulation += Time::DeltaTime();
    while (physicsUpdateDeltaAccumulation >= DeltaTime)
    {
        scene->PrePhysicsTick();

        physicsSystem.Update(DeltaTime, CollisionSteps, &temp_allocator, &job_system);
        physicsUpdateDeltaAccumulation -= DeltaTime;

        // Step the world
        for (auto& b : bodies)
        {
            b.second->UpdateGameObject();
        }
    }
}

void PhysicsScene::DebugDraw()
{
    JPH::BodyManager::DrawSettings drawSettings;
    drawSettings.mDrawShapeWireframe = true;

    bodyDrawFilter.drawRequested.clear();
    for (auto& b : bodies)
    {
        if (b.second->debugDrawRequest || JoltDebugRenderer::GetDrawAll())
        {
            b.second->debugDrawRequest = false;
            bodyDrawFilter.drawRequested.emplace(b.second->GetBody()->GetID());
        }
    }

    physicsSystem.DrawBodies(drawSettings, JoltDebugRenderer::GetDebugRenderer().get(), &bodyDrawFilter);
}

void PhysicsContactListener::OnContactAdded(
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    const JPH::ContactManifold& inManifold,
    JPH::ContactSettings& ioSettings
)
{
    // crash, maybe mutithreading issue
    // auto body1 = reinterpret_cast<PhysicsBody*>(inBody1.GetUserData());
    // auto body2 = reinterpret_cast<PhysicsBody*>(inBody2.GetUserData());

    // if (body1) body1->InvokeContactAddedEvent(body2, inManifold, ioSettings);
    // if (body2) body2->InvokeContactAddedEvent(body1, inManifold, ioSettings);

    // contactingBodies[inBody1.GetID()] = body1;
    // contactingBodies[inBody2.GetID()] = body2;
}

void PhysicsContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
{
    // crash, maybe mutithreading issue
    // auto body1 = contactingBodies[inSubShapePair.GetBody1ID()];
    // auto body2 = contactingBodies[inSubShapePair.GetBody2ID()];

    // if(body1) body1->InvokeContactRemovedEvent(body2);
    // if(body2) body2->InvokeContactRemovedEvent(body1);
}

void PhysicsScene::AddPhysicsBody(PhysicsBody& body)
{
    auto jphBody = body.GetBody();
    assert(jphBody != nullptr);

    bodies[jphBody->GetID()] = &body;
    optimizeNeeded = true;
}

void PhysicsScene::RemovePhysicsBody(PhysicsBody& body)
{
    auto jphBody = body.GetBody();
    assert(jphBody != nullptr);
    bodies.erase(jphBody->GetID());
    optimizeNeeded = true;
}
