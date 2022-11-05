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

            void UploadBuffer(RefPtr<VKBuffer> buffer, uint32_t dstOffset, size_t dataSize, DataRange data[], uint32_t rangeCount);
            void UploadImage(RefPtr<VKImage> image, uint32_t imageSize, void* data);

            void CreateBuffer(VkBufferCreateInfo& createInfo, VmaAllocationCreateInfo& allocCreateInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
            void CreateBuffer(VkBufferCreateInfo& createInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
            void CreateImage(VkImageCreateInfo& imageCreateInfo, VkImage& image, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
            void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
            void DestoryImage(VkImage image, VmaAllocation allocation);

            void RecordPendingCommands(VkCommandBuffer cmd);
            void DestroyPendingResources();
            RefPtr<VKDevice> GetDevice() {return device;}

        private:
            VmaAllocator allocator_vma;

            RefPtr<VKDevice> device;
            uint32_t queueFamilyIndex;

            std::vector<std::function<void(VkCommandBuffer)>> pendingCommands;

            std::vector<std::pair<VkBuffer, VmaAllocation>> pendingBuffersToDelete;
            std::vector<std::pair<VkImage, VmaAllocation>> pendingImagesToDelete;

            std::vector<std::function<void()>> cleanUps;

            VkBuffer GetStageBuffer(uint32_t size,
                VmaAllocation& allocation,
                VmaAllocationInfo& allocationInfo);
   };
}
