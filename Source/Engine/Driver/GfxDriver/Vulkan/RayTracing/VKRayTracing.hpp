#pragma once
#include "Engine/Driver/GfxDriver/RayTracingContext.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/Internal/VKMemAllocator.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKBuffer.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include "Engine/Library/ObjectPool.hpp"

namespace Gfx
{
class VKCommandBuffer;
}

namespace Gfx::VKRayTracing
{
class Manager
{
public:
    Manager();
    ~Manager();
    RayTracingSceneHandle CreateScene(uint32_t maxInstanceCount);
    void DestroyScene(RayTracingSceneHandle scene);

    RayTracingMeshHandle CreateBLAS(std::span<BlasGeometry> geometries);

    RayTracingInstanceHandle CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform, uint32_t customIndex);

    void BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances);

    void BuildSceneCommandBufferImpl(VkCommandBuffer cmd, RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances);
    void CreateBLASCommandBufferImpl(VkCommandBuffer cmd, RayTracingMeshHandle& blasHandle, std::span<VkAccelerationStructureGeometryKHR> vkGeometries, std::vector<uint32_t> maxPrimitiveCounts);

    void* GetNativeHandle(RayTracingSceneHandle scene);

    std::unique_ptr<VKCommandBuffer> cmdBuffer;

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
};
} // namespace Gfx::VKRayTracing
