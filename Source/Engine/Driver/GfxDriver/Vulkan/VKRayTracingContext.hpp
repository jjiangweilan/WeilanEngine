#pragma once
#include "../RayTracingContext.hpp"
#include "RayTracing/VKRayTracing.hpp"

namespace Gfx
{
class VKRayTracingContext : public RayTracingContext
{
public:
    VKRayTracingContext(VKRayTracing::Manager* manager, std::mutex& driverMutex) : manager(manager), driverMutex(driverMutex) {}
    RayTracingSceneHandle CreateScene(uint32_t maxInstanceCount) override;
    RayTracingMeshHandle CreateBLAS(std::span<BlasGeometry> geometries) override;
    RayTracingInstanceHandle CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform) override;
    void BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances) override;
    void* GetNativeHandle(RayTracingSceneHandle scene);

private:
    VKRayTracing::Manager* manager;
    std::mutex& driverMutex;
};
} // namespace Gfx
