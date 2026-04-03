#include "VKRayTracing.hpp"

#include "Engine/Driver/GfxDriver/Vulkan/VKCommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKContext.hpp"
#include <cstring>
#include <spdlog/spdlog.h>
#include <vector>

namespace Gfx::VKRayTracing
{
RayTracingMeshHandle Manager::CreateBLAS(std::span<BlasGeometry> geometries)
{
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;
    auto vkContext = VKContext::Instance();

    auto blasHandle = blasPool.AllocateRaw();
    auto& blasEntry = blasPool[blasHandle];

    std::vector<VkAccelerationStructureGeometryKHR> vkGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
    std::vector<uint32_t> maxPrimitiveCounts;

    static auto validateVertexFormat = [](Gfx::GfxFormat format) -> VkFormat
    {
        switch (format)
        {
            case Gfx::GfxFormat::R32G32B32_SFloat:
                return VK_FORMAT_R32G32B32_SFLOAT;
            default:
                return VK_FORMAT_UNDEFINED;
        }
    };

    for (const auto& geometry : geometries)
    {
        auto vkVertexFormat = validateVertexFormat(geometry.vertexFormat);
        if (vkVertexFormat == VK_FORMAT_UNDEFINED)
        {
            spdlog::warn("Unsupported vertex format for ray tracing: {}", static_cast<int>(geometry.vertexFormat));
            continue;
        }

        VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = vkVertexFormat,
            .vertexData = {.deviceAddress = geometry.vertexBufferShaderDeviceAddress},
            .vertexStride = geometry.vertexStride,
            .maxVertex = geometry.maxVertex,
            .indexType = geometry.indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32,
            .indexData = {.deviceAddress = geometry.indexBufferShaderDeviceAddress}
        };

        vkGeometries.push_back(VkAccelerationStructureGeometryKHR{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR, .geometry = {.triangles = trianglesData}, .flags = VK_GEOMETRY_OPAQUE_BIT_KHR});

        buildRangeInfos.push_back(VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = geometry.triangleCount, .primitiveOffset = 0, .firstVertex = 0, .transformOffset = 0});

        maxPrimitiveCounts.push_back(geometry.triangleCount);
    }

    if (vkGeometries.empty())
        return -1;

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = static_cast<uint32_t>(vkGeometries.size()),
        .pGeometries = vkGeometries.data(),
    };

    // Query memory usage
    VkAccelerationStructureBuildSizesInfoKHR blasBuildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &blasBuildGeometryInfo,
        maxPrimitiveCounts.data(),
        &blasBuildSizes
    );

    // Create blas buffer
    VkBufferCreateInfo blasBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = blasBuildSizes.accelerationStructureSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo blasAllocInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY};
    allocator->CreateBuffer(blasBufferCreateInfo, blasAllocInfo, blasEntry.buffer, blasEntry.allocation);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasEntry.buffer,
        .offset = 0,
        .size = blasBuildSizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    };
    vkCreateAccelerationStructureKHR(device, &blasCreateInfo, nullptr, &blasEntry.handle);

    cmdBuffer->BuildBLAS(blasHandle, vkGeometries, maxPrimitiveCounts);

    return blasHandle;
}

RayTracingInstanceHandle Manager::CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform, uint32_t customIndex)
{
    auto device = VKContext::Instance()->device;
    auto& blas = blasPool[mesh];

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas.handle
    };
    VkDeviceAddress blasDeviceAddr = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);

    const glm::float3x4& m = glm::transpose(initialTransform);
    VkTransformMatrixKHR tm{};
    memcpy(&tm, &m, sizeof(tm));

    VkAccelerationStructureInstanceKHR instance{
        .transform = tm,
        .instanceCustomIndex = customIndex,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blasDeviceAddr
    };

    int instanceHandle = instancePool.AllocateRaw();
    instancePool[instanceHandle].data = instance;

    return instanceHandle;
}

void Manager::BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instanceHandles)
{
    cmdBuffer->BuildTLAS(sceneHandle, instanceHandles);
}

RayTracingSceneHandle Manager::CreateScene(uint32_t maxInstanceCount)
{
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;

    auto sceneHandle = tlasPool.AllocateRaw();
    auto& tlas = tlasPool[sceneHandle];
    tlas.instanceCount = maxInstanceCount;

    // 1. Create instance data buffer
    size_t bufferSize = sizeof(VkAccelerationStructureInstanceKHR) * maxInstanceCount;
    VkBufferCreateInfo instanceBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo instanceAllocInfo{.usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
    allocator->CreateBuffer(instanceBufferCreateInfo, instanceAllocInfo, tlas.instanceBuffer, tlas.instanceAllocation);

    VkBufferDeviceAddressInfo instanceAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = tlas.instanceBuffer
    };
    VkDeviceAddress instanceDeviceAddr = vkGetBufferDeviceAddress(device, &instanceAddrInfo);

    // 2. Describe TLAS Geometry
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = {.deviceAddress = instanceDeviceAddr}
    };

    VkAccelerationStructureGeometryKHR tlasGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instancesData},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry,
    };

    // 3. Query Sizes
    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo, &maxInstanceCount, &tlasBuildSizes);

    // 4. Create TLAS buffer and handle
    VkBufferCreateInfo tlasBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = tlasBuildSizes.accelerationStructureSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo tlasAllocInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY};
    allocator->CreateBuffer(tlasBufferCreateInfo, tlasAllocInfo, tlas.buffer, tlas.allocation);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlas.buffer,
        .size = tlasBuildSizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, nullptr, &tlas.handle);
    tlas.tlasSize = tlasBuildSizes.accelerationStructureSize;

    return sceneHandle;
}

void Manager::DestroyScene(RayTracingSceneHandle sceneHandle)
{
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;
    auto& tlas = tlasPool[sceneHandle];

    if (tlas.handle != VK_NULL_HANDLE)
    {
        vkDestroyAccelerationStructureKHR(device, tlas.handle, nullptr);
        allocator->DestroyBuffer(tlas.buffer, tlas.allocation);
        allocator->DestroyBuffer(tlas.instanceBuffer, tlas.instanceAllocation);
    }

    tlasPool.FreeRaw(sceneHandle);
}

void* Manager::GetNativeHandle(RayTracingSceneHandle scene)
{
    return (void*)tlasPool[scene].handle;
}

void Manager::BuildSceneCommandBufferImpl(VkCommandBuffer cmd, RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instanceHandles)
{
    auto& tlas = tlasPool[sceneHandle];
    auto vkContext = VKContext::Instance();
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;

    uint32_t instanceCount = static_cast<uint32_t>(instanceHandles.size());
    if (instanceCount == 0 || instanceCount > tlas.instanceCount)
    {
        spdlog::warn("Instance count is zero so it({}) exceeds the maximum instance count {} for this TLAS", instanceCount, tlas.instanceCount);
        return;
    }

    auto instanceDataBuffer = allocator->AllocateScratchBuffer(sizeof(VkAccelerationStructureInstanceKHR) * instanceCount, 16, VKMemAllocator::ScratchBuffer::ScratchBufferUsage::HostVisibleScatchBuffer);
    void* mappedData = instanceDataBuffer.mappedData;
    auto* instanceData = static_cast<VkAccelerationStructureInstanceKHR*>(mappedData);
    for (uint32_t i = 0; i < instanceCount; ++i)
    {
        instanceData[i] = instancePool[instanceHandles[i]].data;
    }
    VkDeviceAddress instanceDeviceAddr = instanceDataBuffer.deviceAddress; // vkGetBufferDeviceAddress(device, &instanceAddrInfo);

    // 2. Describe TLAS Geometry
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = {.deviceAddress = instanceDeviceAddr}
    };

    VkAccelerationStructureGeometryKHR tlasGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instancesData},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = 0,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = tlas.handle,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry,
    };

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo, &instanceCount, &tlasBuildSizes);

    auto scratchBufferHandle = allocator->AllocateScratchBuffer(tlasBuildSizes.buildScratchSize, vkContext->gpu->asProps.minAccelerationStructureScratchOffsetAlignment, VKMemAllocator::ScratchBuffer::ScratchBufferUsage::GPUScratchBuffer);
    tlasBuildGeometryInfo.scratchData.deviceAddress = scratchBufferHandle.deviceAddress; // vkGetBufferDeviceAddress(device, &scratchAddrInfo);

    VkCommandBuffer commandBuffer = vkContext->currentFrameContext->cmd;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = instanceCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlasBuildGeometryInfo, &pBuildRangeInfo);

    VkMemoryBarrier2 memBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    // full pipeline barrier
    VkDependencyInfo info =
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memBarrier
        };
    vkCmdPipelineBarrier2(commandBuffer, &info);
}

void Manager::CreateBLASCommandBufferImpl(VkCommandBuffer cmd, RayTracingMeshHandle& blasHandle, std::span<VkAccelerationStructureGeometryKHR> vkGeometries, std::vector<uint32_t> maxPrimitiveCounts)
{
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;
    auto vkContext = VKContext::Instance();
    auto blasEntry = blasPool[blasHandle];

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};

    int geometryIndex = 0;
    for (auto& geometry : vkGeometries)
    {
        buildRangeInfos.push_back(VkAccelerationStructureBuildRangeInfoKHR{.primitiveCount = maxPrimitiveCounts[geometryIndex], .primitiveOffset = 0, .firstVertex = 0, .transformOffset = 0});
        geometryIndex += 1;
    }

    if (vkGeometries.empty())
        return;

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = static_cast<uint32_t>(vkGeometries.size()),
        .pGeometries = vkGeometries.data(),
    };

    //// Query memory usage
    VkAccelerationStructureBuildSizesInfoKHR blasBuildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &blasBuildGeometryInfo,
        maxPrimitiveCounts.data(),
        &blasBuildSizes
    );

    blasBuildGeometryInfo.dstAccelerationStructure = blasEntry.handle;

    auto scratchBufferHandle = allocator->AllocateScratchBuffer(blasBuildSizes.buildScratchSize, vkContext->gpu->asProps.minAccelerationStructureScratchOffsetAlignment, VKMemAllocator::ScratchBuffer::ScratchBufferUsage::GPUScratchBuffer);
    blasBuildGeometryInfo.scratchData.deviceAddress = scratchBufferHandle.deviceAddress; // vkGetBufferDeviceAddress(device, &scratchAddrInfo);

    auto commandBuffer = vkContext->currentFrameContext->cmd;
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = buildRangeInfos.data();
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &blasBuildGeometryInfo, &pBuildRangeInfos);

    VkMemoryBarrier2 memBarrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    // full pipeline barrier
    VkDependencyInfo info =
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memBarrier
        };
    vkCmdPipelineBarrier2(commandBuffer, &info);
}

Manager::Manager()
{
    cmdBuffer = std::make_unique<VKCommandBuffer>(nullptr); // nullptr is ok, because we don't allocate any dynamic resources
}

Manager::~Manager()
{
    for (auto& tlas : tlasPool)
    {
        if (tlas.handle != VK_NULL_HANDLE)
        {
            auto device = VKContext::Instance()->device;
            auto allocator = VKContext::Instance()->allocator;

            vkDestroyAccelerationStructureKHR(device, tlas.handle, nullptr);
            allocator->DestroyBuffer(tlas.buffer, tlas.allocation);
            allocator->DestroyBuffer(tlas.instanceBuffer, tlas.instanceAllocation);
        }
    }

    for (auto& blas : blasPool)
    {
        if (blas.handle != VK_NULL_HANDLE)
        {
            auto device = VKContext::Instance()->device;
            auto allocator = VKContext::Instance()->allocator;

            vkDestroyAccelerationStructureKHR(device, blas.handle, nullptr);
            allocator->DestroyBuffer(blas.buffer, blas.allocation);
        }
    }
}
} // namespace Gfx::VKRayTracing
