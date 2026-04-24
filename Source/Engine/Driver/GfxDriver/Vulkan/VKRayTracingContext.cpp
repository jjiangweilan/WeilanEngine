#include "VKRayTracingContext.hpp"

namespace Gfx
{

RayTracingSceneHandle VKRayTracingContext::CreateScene(uint32_t maxInstanceCount)
{
    std::scoped_lock lock(driverMutex);
    return manager->CreateScene(maxInstanceCount);
}

RayTracingMeshHandle VKRayTracingContext::CreateBLAS(std::span<BlasGeometry> geometries)
{
    std::scoped_lock lock(driverMutex);
    return manager->CreateBLAS(geometries);
}

RayTracingInstanceHandle VKRayTracingContext::CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform, uint32_t customIndex)
{
    std::scoped_lock lock(driverMutex);
    return manager->CreateInstance(mesh, initialTransform, customIndex);
}

void VKRayTracingContext::UpdateInstanceTransform(RayTracingInstanceHandle instance, glm::float4x3 transform)
{
    std::scoped_lock lock(driverMutex);
    manager->UpdateInstanceTransform(instance, transform);
}

void VKRayTracingContext::BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances)
{
    std::scoped_lock lock(driverMutex);
    manager->BuildScene(sceneHandle, instances);
}

void* VKRayTracingContext::GetNativeHandle(RayTracingSceneHandle scene)
{
    return manager->GetNativeHandle(scene);
}

} // namespace Gfx
