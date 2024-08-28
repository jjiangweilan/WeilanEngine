#pragma once
#include "../CompiledSpv.hpp"
#include "../DescriptorSetSlot.hpp"
#include "../ShaderProgram.hpp"
#include "VKShaderInfo.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
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
    static VkSamplerCreateInfo GenerateSamplerCreateInfoFromString(
        const std::string& lowerBindingName, bool enableCompare
    );

private:
    static std::unordered_map<vk::SamplerCreateInfo, VkSampler> samplers;
};

class VKShaderProgram : public ShaderProgram
{
public:
    using SetNum = uint32_t;
    VKShaderProgram(
        std::shared_ptr<const ShaderConfig> config,
        VKContext* context,
        const std::string& name,
        CompiledSpv& compiledSpv
    );

    VKShaderProgram(const VKShaderProgram& other) = delete;
    ~VKShaderProgram() override;

    VkPipelineLayout GetVKPipelineLayout();
    bool HasSet(uint32_t set)
    {
        return GetLayoutHash(set) != 0;
    }

    // request a pipeline object according to config
    // we may have slightly different pipelines with different configs, VKShader should cache these pipelines(TODO)
    VkPipeline RequestGraphicsPipeline(const ShaderConfig& config, VKRenderPass* renderPass, uint32_t subpass);
    VkPipeline RequestComputePipeline(const ShaderConfig& config);
    VKDescriptorPool& GetDescriptorPool(DescriptorSetSlot slot);

    const ShaderConfig& GetDefaultShaderConfig() override;

    const ShaderInfo::ShaderInfo& GetShaderInfo() override
    {
        return shaderInfo;
    }
    const std::string& GetName() override
    {
        return name;
    }

    // note layout created with VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR won't register to layouy hashes
    size_t GetLayoutHash(uint32_t set);

private:
    struct DescriptorSetLayoutBindingWrap
    {
        std::vector<VkDescriptorSetLayoutBinding> binding;
        std::vector<std::vector<VkSampler>> samplers;
    };
    typedef std::unordered_map<SetNum, DescriptorSetLayoutBindingWrap> DescriptorSetBindings;
    typedef std::vector<std::unordered_map<VkDescriptorType, VkDescriptorPoolSize>> PoolSizeMap;
    struct PipelineCache
    {
        VkPipeline pipeline;
        VkRenderPass renderPass;
        uint32_t subpass;
        ShaderConfig config;
    };

    std::string name = "";
    ShaderInfo::ShaderInfo shaderInfo = {};
    VKObjectManager* objManager = nullptr;
    std::unique_ptr<VKShaderModule> vertShaderModule = nullptr;
    std::unique_ptr<VKShaderModule> fragShaderModule = nullptr;
    std::unique_ptr<VKShaderModule> computeShaderModule = nullptr;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<PipelineCache> caches = {};
    std::vector<VkSampler> immutableSamplers = {};
    std::vector<size_t> layoutHash = {};
    std::vector<RefPtr<VKDescriptorPool>> descriptorPools = {};
    VkDescriptorSet descriptorSet;

    // descriptor pool take a pointer to these value so these can't be temp values
    DescriptorSetBindings descriptorSetBindings = {};

    void CreateShaderPipeline(std::shared_ptr<const ShaderConfig> config, VKShaderModule* fallbackConfigModule);
    void GeneratePipelineLayoutAndGetDescriptorPool(DescriptorSetBindings& combined);

    std::shared_ptr<const ShaderConfig> defaultShaderConfig = {};
};
} // namespace Gfx
