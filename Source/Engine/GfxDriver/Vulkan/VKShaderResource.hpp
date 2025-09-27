#pragma once
#include "../DescriptorSetSlot.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/Vulkan/VKImageView.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKShaderInfo.hpp"
#include "VKSharedResource.hpp"
#include <unordered_map>
#include <variant>
#include <vk_mem_alloc.h>
namespace Gfx
{
class VKBuffer;
class VKImage;
class VKShaderProgram;
class VKDescriptorPool;
class VKDriver;
class VKCommandBufferProcessor;

struct VKWritableGPUResource
{
    enum class Type
    {
        Buffer,
        Image
    };
    Type type;
    std::variant<ObjPtr<Image>, ObjPtr<Buffer>> data;

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
    VKImage* targetImage = nullptr;
};

// TODO:
// 1. improve when we should BuildAll
// 2. descriptor set should be release when it's not used anymore
class VKShaderResource : public ShaderResource
{
    // ---------------------------- New API ----------------------------------
public:
    void SetName(std::string_view name) override;
    const std::string& GetName() override { return name; }

    VKShaderResource();
    VKShaderResource(const VKShaderResource& other) = delete;
    ~VKShaderResource() override;
    void SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer) override;

    // Note: don't bind swapchain image, we didn't handle it (it's actually multiple images)
    void SetImage(ShaderBindingHandle handle, int index, Gfx::Image* image) override;
    void SetImage(ShaderBindingHandle handle, int index, Gfx::ImageView* imageView) override;
    void SetImage(ShaderBindingHandle handle, int index, const Gfx::RG::ImageIdentifier& imageId) override;
    void Remove(ShaderBindingHandle handle) override;
    void Clear() override;
    void RebuildAll() override;

    VkDescriptorSet GetDescriptorSet(uint32_t set, VKShaderProgram* shaderProgram, VKCommandBufferProcessor* graph);
    const std::vector<VKWritableGPUResource>& GetWritableResources(
        uint32_t set, VKShaderProgram* shaderProgram, VKCommandBufferProcessor* graph
    );

protected:
    enum class ShaderBindingType
    {
        None,
        ImageView,
        Buffer,
        ImageID,
    };

    struct ResourceRef
    {
        bool IsValidRef()
        {
            if (type == ShaderBindingType::None)
                return false;
            else if (type == ShaderBindingType::Buffer)
                return std::get<ObjPtr<Buffer>>(res) != nullptr;
            else if (type == ShaderBindingType::ImageView)
                return std::get<ObjPtr<ImageView>>(res) != nullptr;
            else if (type == ShaderBindingType::ImageID)
                return true;

            return false;
        }

        void* GetRef();

        bool IsImageView() const { return type == ShaderBindingType::ImageView; }

        const Gfx::RG::ImageIdentifier& GetID() const { return res.index() == 2 ? std::get<Gfx::RG::ImageIdentifier>(res) : Gfx::RG::ImageIdentifier::GetEmpty(); }

        std::variant<ObjPtr<ImageView>, ObjPtr<Buffer>, Gfx::RG::ImageIdentifier> res = ObjPtr<ImageView>(nullptr);
        ShaderBindingType type = ShaderBindingType::None;
    };

    struct SetInfo
    {
        VKShaderProgram* program;
        ObjPtr<VKDescriptorPool> descriptorPool = nullptr;
        uint32_t creationSetIndex;
        VkDescriptorSet set;
        bool rebuild = false;
        std::vector<VKWritableGPUResource> writableGPUResources;
    };

    struct SetGroup
    {
        UUID id;
        uint32_t set;
        bool operator==(const SetGroup& other) const
        {
            return id == other.id && set == other.set;
        }
    };

    struct SetGroupHash
    {
        size_t operator()(const SetGroup& group) const;
    };

    // first key: binding name
    // second key: array index
    std::unordered_map<ShaderBindingHandle, std::unordered_map<int, ResourceRef>> bindings;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VKSharedResource* sharedResource;
    std::unordered_map<SetGroup, SetInfo, SetGroupHash> sets;
    std::unique_ptr<VKBuffer> defaultBuffer;
    std::string name;

    void SetNameInternal(std::string_view name, VKShaderProgram* shader, VkDescriptorSet set, int setIndex);
};
} // namespace Gfx
