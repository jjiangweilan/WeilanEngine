#pragma once
#include "../CompiledSpv.hpp"
#include "../DescriptorSetSlot.hpp"
#include "../ShaderProgram.hpp"
#include "Driver/GfxDriver/VertexAttributes.hpp"
#include "Driver/GfxDriver/Vulkan/Internal/VKMemAllocator.hpp"
#include "Library/DynamicArray.hpp"
#include "VKShaderInfo.hpp"
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_hash.hpp>
namespace Gfx
{
class VKShaderModule;
class VKSwapChain;
class VKObjectManager;
class VKDescriptorPool;
class VKShaderBufferStrategy;
class VKContext;
class VKRenderPass;
struct ShaderPushConstant;
class SamplerCachePool
{
public:
    static VkSampler RequestSampler(VkSamplerCreateInfo& createInfo);
    static void DestroyPool();
    static VkSamplerCreateInfo GenerateSamplerCreateInfo(const Gfx::ShaderPipelineInfo::SamplerConfig& samplerConfig);

private:
    static std::unordered_map<vk::SamplerCreateInfo, VkSampler> samplers;
};

class VKShaderProgram : public ShaderProgram
{
    DECLARE_OBJECT();

public:
    using SetNum = uint32_t;
    VKShaderProgram();
    VKShaderProgram(VKContext* context, const PipelineCreateInfo& createInfo);

    VKShaderProgram(const VKShaderProgram& other) = delete;
    ~VKShaderProgram() override;

    VkPipelineLayout GetVKPipelineLayout();

    VkPipeline RequestGraphicsPipeline(
        const PipelineConfig& config,
        std::span<VKBuffer*> vertexBindingBuffers,
        VKRenderPass* renderPass,
        uint32_t subpass,
        int requirePushDescriptorSet
    );
    VkPipeline RequestComputePipeline(int requirePushDescriptorSet);
    VKDescriptorPool* GetDescriptorPool(DescriptorSetSlot slot);

    // std::shared_ptr<const ShaderConfig> GetDefaultShaderConfig() override;

    int GetBindingNum(Gfx::DescriptorSetSemantics descriptorSet, std::string_view name) override;
    const PipelineConfig& GetDefaultShaderConfig() override { return defaultPipelineConfig; };
    const ShaderPipelineInfo& GetShaderInfo() override { return pipelineInfo; }
    const std::string& GetName() const override { return name; }
    bool HasSet(int set) const { return set >= 0 && set < pipelineInfo.descriptorSets.size(); }

private:
    using PipelineRequestHash = uint64_t;

    struct DescriptorSetLayoutBindingWrap
    {
        std::vector<VkDescriptorSetLayoutBinding> binding;
        std::vector<std::vector<VkSampler>> samplers;
    };
    typedef std::unordered_map<SetNum, DescriptorSetLayoutBindingWrap> DescriptorSetBindings;
    typedef std::vector<std::unordered_map<VkDescriptorType, VkDescriptorPoolSize>> PoolSizeMap;
    int isPushDescriptorSetCompatible = false;

    std::string name = "";
    VKObjectManager* objManager = nullptr;
    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkShaderModule computeModule = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::unordered_map<PipelineRequestHash, std::pair<ObjPtr<VKRenderPass>, VkPipeline>> caches = {};
    std::vector<VKDescriptorPool*> descriptorPools = {};
    ShaderPipelineInfo pipelineInfo;
    PipelineConfig defaultPipelineConfig;

    // descriptor pool take a pointer to these value so these can't be temp values
    DescriptorSetBindings descriptorSetBindings = {};

    // void CreateShaderPipeline(std::shared_ptr<const ShaderConfig> config, VKShaderModule* fallbackConfigModule);
    void GeneratePipelineLayout();
    void GeneratePipelineLayoutAndGetDescriptorPool(DescriptorSetBindings& combined);
    void CleanUpInvalidCaches();
};
} // namespace Gfx
