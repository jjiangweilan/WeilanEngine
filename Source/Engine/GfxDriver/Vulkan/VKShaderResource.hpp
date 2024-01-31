#pragma once
#include "../DescriptorSetSlot.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/Vulkan/VKImageView.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKShaderInfo.hpp"
#include "VKSharedResource.hpp"
#include <unordered_map>
#include <vma/vk_mem_alloc.h>

namespace Gfx
{
class VKBuffer;
class VKImage;
class VKShaderProgram;
class VKDescriptorPool;
class VKDriver;

struct VKWritableGPUResource
{
    enum class Type
    {
        Buffer,
        Image
    };
    Type type;
    void* data;
    VkPipelineStageFlags stages;
    VkAccessFlags access;

    VKImageView* imageView;
    VkImageLayout layout;
};

struct Barrier
{
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    uint32_t barrierCount;
    int memoryBarrierIndex = -1;
    int bufferMemoryBarrierIndex = -1;
    int imageMemorybarrierIndex = -1;
};

// TODO:
// 1. improve when we should BuildAll
// 2. descriptor set should be release when it's not used anymore
class VKShaderResource : public ShaderResource
{
    // ---------------------------- New API ----------------------------------
public:
    VKShaderResource();
    VKShaderResource(const VKShaderResource& other) = delete;
    void SetBuffer(ResourceHandle handle, Gfx::Buffer* buffer) override;

    // Note: don't bind swapchain image, we didn't handle it (it's actually multiple images)
    void SetImage(ResourceHandle handle, Gfx::Image* image) override;
    void SetImage(ResourceHandle handle, Gfx::ImageView* imageView) override;
    void SetImage(ResourceHandle handle, nullptr_t) override;

    VkDescriptorSet GetDescriptorSet(uint32_t set, VKShaderProgram* shaderProgram);
    const std::vector<VKWritableGPUResource>& GetWritableResources(uint32_t set, VKShaderProgram* shaderProgram);

    // ---------------------------- Old API ----------------------------------
public:
    ~VKShaderResource() override;

protected:
    enum class ResourceType
    {
        ImageView,
        Buffer
    };

    struct ResourceRef
    {
        void* res = nullptr;
        ResourceType type;
    };
    std::unordered_map<ResourceHandle, ResourceRef> bindings;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    VKSharedResource* sharedResource;

    VKDescriptorPool* descriptorPool = nullptr;

    struct SetInfo
    {
        uint32_t creationSetIndex;
        VkDescriptorSet set = VK_NULL_HANDLE;
        bool rebuild = false;
        std::vector<VKWritableGPUResource> writableGPUResources;
    };
    std::unordered_map<VKShaderProgram*, SetInfo> sets;

    void RebuildAll();
};
} // namespace Gfx
