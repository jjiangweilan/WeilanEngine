#include "Physics.hpp"
#include <spdlog/spdlog.h>

void Physics::Init()
{
    // https://nvidia-omniverse.github.io/PhysX/physx/5.3.1/docs/Startup.html

    foundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
    if (!foundation)
        SPDLOG_CRITICAL("PxCreateFoundation failed!");

    bool recordMemoryAllocations = false;
    // The optional PVD instance enables the debugging and profiling with the PhysX Visual Debugger
    physx::PxPvd* pvd = nullptr; // PxCreatePvd(*foundation);
    // physx::PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
    // mPvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);

    physics =
        PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale(), recordMemoryAllocations, pvd);
    if (!physics)
        SPDLOG_CRITICAL("PxCreatePhysics failed!");

    if (!PxInitExtensions(*physics, pvd))
        SPDLOG_CRITICAL("PxInitExtensions failed!");
}
void Physics::Destroy()
{
    physics->release();
    foundation->release();
};
