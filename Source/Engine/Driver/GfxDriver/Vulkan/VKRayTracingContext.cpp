#include "VKRayTracingContext.hpp"

namespace Gfx
{

RayTracingSceneHandle VKRayTracingContext::CreateScene(uint32_t maxInstanceCount)
{
    return manager.CreateScene(maxInstanceCount);
}

RayTracingMeshHandle VKRayTracingContext::CreateBLAS(std::span<BlasGeometry> geometries)
{
    return manager.CreateBLAS(geometries);
}

RayTracingInstanceHandle VKRayTracingContext::CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform)
{
    return manager.CreateInstance(mesh, initialTransform);
}

void VKRayTracingContext::BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances)
{
    manager.BuildScene(sceneHandle, instances);
}

void* VKRayTracingContext::GetNativeHandle(RayTracingSceneHandle scene)
{
    return manager.GetNativeHandle(scene);
}

} // namespace Gfx
