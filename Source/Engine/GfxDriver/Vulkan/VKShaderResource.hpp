#pragma once
#include "../DescriptorSetSlot.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/Vulkan/VKImageView.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKShaderInfo.hpp"
#include "VKSharedResource.hpp"
#include <unordered_map>
#include <vk_mem_alloc.h>
#include <variant>
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
    std::variant<SRef<Image>, SRef<Buffer>> data;

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
    void SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer) override;

    // Note: don't bind swapchain image, we didn't handle it (it's actually multiple images)
    void SetImage(ShaderBindingHandle handle, int index, Gfx::Image* image) override;
    void SetImage(ShaderBindingHandle handle, int index, Gfx::ImageView* imageView) override;
    void Remove(ShaderBindingHandle handle) override;

    VkDescriptorSet GetDescriptorSet(uint32_t set, VKShaderProgram* shaderProgram);
    const std::vector<VKWritableGPUResource>& GetWritableResources(uint32_t set, VKShaderProgram* shaderProgram);

    // ---------------------------- Old API ----------------------------------
public:
    ~VKShaderResource() override;

protected:
    enum class ShaderBindingType
    {
        None,
        ImageView,
        Buffer
    };

    struct ResourceRef
    {
        bool IsValidRef()
        {
            if (type == ShaderBindingType::None) return false;
            else if (type == ShaderBindingType::Buffer)
                return std::get<SRef<Buffer>>(res) != nullptr;
            else if (type == ShaderBindingType::ImageView)
                return std::get<SRef<ImageView>>(res) != nullptr;

            return false;
        }

        void* GetRef()
        {
            if (type == ShaderBindingType::Buffer)
                return std::get<SRef<Buffer>>(res).Get();
            else if (type == ShaderBindingType::ImageView)
                return std::get<SRef<ImageView>>(res).Get();

            return nullptr;
        }

        std::variant<SRef<ImageView>, SRef<Buffer>> res = SRef<ImageView>(nullptr);
        ShaderBindingType type = ShaderBindingType::None;
    };

    // first key: binding name
    // second key: array index
    std::unordered_map<ShaderBindingHandle, std::unordered_map<int, ResourceRef>> bindings;
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

    std::unique_ptr<VKBuffer> defaultBuffer;

    void RebuildAll();
};
} // namespace Gfx
