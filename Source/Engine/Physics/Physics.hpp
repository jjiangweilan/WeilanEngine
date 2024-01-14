#pragma once
#include <physx/PxPhysicsAPI.h>

// use this class to initialze physics module (PhysX)
class Physics
{
public:
    void Init();
    void Destroy();

private:
    physx::PxDefaultErrorCallback defaultErrorCallback;
    physx::PxDefaultAllocator defaultAllocatorCallback;
    physx::PxPhysics* physics;

    physx::PxFoundation* foundation;
};
