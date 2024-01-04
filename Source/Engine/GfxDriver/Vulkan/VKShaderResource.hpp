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

// actually a binding resource
class VKShaderResource : public ShaderResource
{
    // ---------------------------- New API ----------------------------------
public:
    VKShaderResource();
    void SetBuffer(const std::string& name, Gfx::Buffer* buffer) override;
    void SetImage(const std::string& name, Gfx::Image* image) override;
    void SetImage(const std::string& name, Gfx::ImageView* imageView) override;
    void SetImage(const std::string& name, nullptr_t) override;
    VkDescriptorSet GetDescriptorSet(uint32_t set, VKShaderProgram* shaderProgram);

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
    std::unordered_map<std::string, ResourceRef> bindings;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    VKSharedResource* sharedResource;

    VKDescriptorPool* descriptorPool = nullptr;
    std::unordered_map<std::string, std::unique_ptr<VKBuffer>> buffers;

    struct SetInfo
    {
        VkDescriptorSet set = VK_NULL_HANDLE;
        bool rebuild = false;
    };
    std::unordered_map<VKShaderProgram*, SetInfo> sets;

    void RebuildAll();
};
} // namespace Gfx
