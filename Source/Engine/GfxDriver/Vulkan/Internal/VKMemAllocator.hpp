#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <functional>
#include <vector>
#include <utility>
#include "Code/Ptr.hpp"
#include "VKDevice.hpp"

namespace Engine::Gfx
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
            VKMemAllocator(VkInstance instance, RefPtr<VKDevice> device, VkPhysicalDevice physicalDevice, uint32_t transferQueueIndex);
            VKMemAllocator(const VKMemAllocator& other) = delete;
            ~VKMemAllocator();

            //void UploadBuffer(RefPtr<VKBuffer> buffer, uint32_t dstOffset, size_t dataSize, DataRange data[], uint32_t rangeCount);
            //void UploadImage(RefPtr<VKImage> image, uint32_t imageSize, void* data);

            void CreateBuffer(VkBufferCreateInfo& createInfo, VmaAllocationCreateInfo& allocCreateInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
            void CreateBuffer(VkBufferCreateInfo& createInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
            void CreateImage(VkImageCreateInfo& imageCreateInfo, VkImage& image, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
            void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
            void DestoryImage(VkImage image, VmaAllocation allocation);

            RefPtr<VKDevice> GetDevice() {return device;}
            inline VmaAllocator GetHandle() { return allocator_vma; };

            VmaAllocator allocator_vma;
            RefPtr<VKDevice> device;
            uint32_t queueFamilyIndex;

            VkBuffer GetStageBuffer(uint32_t size,
                VmaAllocation& allocation,
                VmaAllocationInfo& allocationInfo);
   };
}
