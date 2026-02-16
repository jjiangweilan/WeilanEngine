#pragma once
#include "Engine/Driver/GfxDriver/RayTracingContext.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKBuffer.hpp"
#include "Engine/Library/CommandStream.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx::VKRayTracing
{

class Manager : public CommandStreamContext, public RayTracingContext
{
public:
    RayTracingSceneHandle CreateScene() override;

    RayTracingMeshHandle CreateBLAS(std::span<BlasGeometry> geometries) override;

    RayTracingInstanceHandle CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform) override;

    void BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances) override;

    void* GetNativeHandle(RayTracingSceneHandle scene) override;

private:
    struct TLAS
    {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        size_t tlasSize = 0;

        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VmaAllocation instanceAllocation = nullptr;
        uint32_t instanceCount = 0;
    };

    struct BLAS
    {
        VkAccelerationStructureKHR handle;
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    struct Instance
    {
        VkAccelerationStructureInstanceKHR data;
    };

    ObjectPool<TLAS> tlasPool;
    ObjectPool<BLAS> blasPool;
    ObjectPool<Instance> instancePool;

    CommandStream commandStream;
};
} // namespace Gfx::VKRayTracing
