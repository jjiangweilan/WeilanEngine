#include "VKRayTracing.hpp"

#include "Engine/Driver/GfxDriver/Vulkan/VKContext.hpp"
#include <cstring>
#include <vector>
#include <spdlog/spdlog.h>
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx::VKRayTracing
{
RayTracingMeshHandle Manager::CreateBLAS(std::span<BlasGeometry> geometries)
{
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;

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
        VKBuffer* vkVertexBuffer = static_cast<VKBuffer*>(geometry.vertexBuffer);
        VKBuffer* vkIndexBuffer = static_cast<VKBuffer*>(geometry.indexBuffer);

        auto vkVertexFormat = validateVertexFormat(geometry.vertexFormat);
        if (vkVertexFormat == VK_FORMAT_UNDEFINED)
        {
            spdlog::warn("Unsupported vertex format for ray tracing: {}", static_cast<int>(geometry.vertexFormat));
            continue;
        }

        VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = vkVertexFormat,
            .vertexData = {.deviceAddress = vkVertexBuffer->GetDeviceAddress()},
            .vertexStride = geometry.vertexStride,
            .maxVertex = geometry.maxVertex,
            .indexType = geometry.indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32,
            .indexData = {.deviceAddress = vkIndexBuffer->GetDeviceAddress()}
        };

        vkGeometries.push_back(VkAccelerationStructureGeometryKHR{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = trianglesData},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        });

        buildRangeInfos.push_back(VkAccelerationStructureBuildRangeInfoKHR{
            .primitiveCount = geometry.triangleCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });

        maxPrimitiveCounts.push_back(geometry.triangleCount);
    }

    if (vkGeometries.empty())
        return 0;

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

    auto blasHandle = blasPool.AllocateRaw();
    auto& blasEntry = blasPool[blasHandle];

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
    blasBuildGeometryInfo.dstAccelerationStructure = blasEntry.handle;

    // Create scratch buffer
    VkBuffer scratchBuffer;
    VmaAllocation scratchAllocation;
    VkBufferCreateInfo scratchBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = blasBuildSizes.buildScratchSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo scratchAllocInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY};
    allocator->CreateBuffer(scratchBufferCreateInfo, scratchAllocInfo, scratchBuffer, scratchAllocation);

    VkBufferDeviceAddressInfo scratchAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = scratchBuffer
    };
    blasBuildGeometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &scratchAddrInfo);

    // Build BLAS
    auto cmdPool = VKContext::Instance()->mainCmdPool;
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = buildRangeInfos.data();
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &blasBuildGeometryInfo, &pBuildRangeInfos);
    vkEndCommandBuffer(commandBuffer);

    // submit
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    vkQueueSubmit(VKContext::Instance()->mainQueue->handle, 1, &submitInfo, VK_NULL_HANDLE);

    // Sync
    vkDeviceWaitIdle(device);
    allocator->DestroyBuffer(scratchBuffer, scratchAllocation);

    return blasHandle;
}

RayTracingInstanceHandle Manager::CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform)
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
        .instanceCustomIndex = 0,
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
    auto& tlas = tlasPool[sceneHandle];
    auto device = VKContext::Instance()->device;
    auto allocator = VKContext::Instance()->allocator;

    uint32_t instanceCount = static_cast<uint32_t>(instanceHandles.size());
    if (instanceCount == 0 || instanceCount > tlas.instanceCount)
        return;

    // 1. Prepare instance data buffer
    void* mappedData;
    vmaMapMemory(allocator->GetHandle(), tlas.instanceAllocation, &mappedData);
    auto* instanceData = static_cast<VkAccelerationStructureInstanceKHR*>(mappedData);
    for (uint32_t i = 0; i < instanceCount; ++i)
    {
        instanceData[i] = instancePool[instanceHandles[i]].data;
    }
    vmaUnmapMemory(allocator->GetHandle(), tlas.instanceAllocation);

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
        .flags = 0,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = tlas.handle,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry,
    };

    // 3. Create Scratch Buffer
    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo, &instanceCount, &tlasBuildSizes);

    VkBuffer scratchBuffer;
    VmaAllocation scratchAllocation;
    VkBufferCreateInfo scratchBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = tlasBuildSizes.buildScratchSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo scratchAllocInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY};
    allocator->CreateBuffer(scratchBufferCreateInfo, scratchAllocInfo, scratchBuffer, scratchAllocation);

    VkBufferDeviceAddressInfo scratchAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = scratchBuffer
    };
    tlasBuildGeometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &scratchAddrInfo);

    // 4. Build Command
    auto cmdPool = VKContext::Instance()->mainCmdPool;
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = instanceCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlasBuildGeometryInfo, &pBuildRangeInfo);
    vkEndCommandBuffer(commandBuffer);

    // submit
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    vkQueueSubmit(VKContext::Instance()->mainQueue->handle, 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(device);

    allocator->DestroyBuffer(scratchBuffer, scratchAllocation);
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
} // namespace Gfx::VKRayTracing
