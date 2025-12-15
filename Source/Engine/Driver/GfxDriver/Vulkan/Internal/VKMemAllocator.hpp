#pragma once

#include "Engine/Core/Ptr.hpp"
#include <functional>
#include <utility>
#include <list>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

namespace Gfx
{
struct DataRange
{
    uint32_t offsetInSrc;
    uint32_t size;
    void* data;
};

class Image;
class VKImage;
class VKBuffer;
class VKMemAllocator
{
public:
    VKMemAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t transferQueueIndex);
    VKMemAllocator(const VKMemAllocator& other) = delete;
    ~VKMemAllocator();

    // void UploadBuffer(RefPtr<VKBuffer> buffer, uint32_t dstOffset, size_t dataSize, DataRange data[], uint32_t
    // rangeCount); void UploadImage(RefPtr<VKImage> image, uint32_t imageSize, void* data);

    void CreateBuffer(
        VkBufferCreateInfo& createInfo,
        VmaAllocationCreateInfo& allocCreateInfo,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        VmaAllocationInfo* allocationInfo = nullptr
    );
    void CreateBuffer(
        VkBufferCreateInfo& createInfo,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        VmaAllocationInfo* allocationInfo = nullptr
    );
    void CreateImage(
        VkImageCreateInfo& imageCreateInfo,
        VkImage& image,
        VmaAllocation& allocation,
        VmaAllocationInfo* allocationInfo = nullptr
    );
    void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
    void DestoryImage(VkImage image, VmaAllocation allocation);

    void DestroyPendingResources(bool destroyAll = false);

    inline VmaAllocator GetHandle()
    {
        return allocator_vma;
    };

    VmaAllocator allocator_vma;
    VkDevice device;
    uint32_t queueFamilyIndex;

    VkBuffer GetStageBuffer(uint32_t size, VmaAllocation& allocation, VmaAllocationInfo& allocationInfo);

private:
    struct Info
    {
        void* ptr;
        VmaAllocation allocation;
        int frameCount;
    };

    std::list<Info> pendingBuffers;
    std::list<Info> pendingImages;
    template <class T, class F>
    void DestroyPendingResourcesOfType(std::list<Info>& resources, F f, bool destroyAll);
};
} // namespace Gfx
