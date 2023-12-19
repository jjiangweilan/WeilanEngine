#pragma once
#include "../DescriptorSetSlot.hpp"
#include "../ShaderProgram.hpp"
#include "VKShaderInfo.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
namespace Gfx
{
class VKShaderModule;
class VKSwapChain;
class VKObjectManager;
class VKDescriptorPool;
class VKShaderBufferStrategy;
class VKContext;
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
    static std::unordered_map<uint32_t, VkSampler> samplers;
};

class VKShaderProgram : public ShaderProgram
{
public:
    using SetNum = uint32_t;
    VKShaderProgram(
        std::shared_ptr<const ShaderConfig> config,
        RefPtr<VKContext> context,
        const std::string& name,
        const unsigned char* vert,
        uint32_t vertSize,
        const unsigned char* frag,
        uint32_t fragSize
    );

    VKShaderProgram(
        std::shared_ptr<const ShaderConfig> config,
        VKContext* context,
        const std::string& name,
        const unsigned char* compute,
        uint32_t computeSize
    );

    VKShaderProgram(
        std::shared_ptr<const ShaderConfig> config,
        RefPtr<VKContext> context,
        const std::string& name,
        const std::vector<uint32_t>& comp
    );

    VKShaderProgram(
        std::shared_ptr<const ShaderConfig> config,
        RefPtr<VKContext> context,
        const std::string& name,
        const std::vector<uint32_t>& vert,
        const std::vector<uint32_t>& frag
    );
    VKShaderProgram(const VKShaderProgram& other) = delete;
    ~VKShaderProgram() override;

    VkPipelineLayout GetVKPipelineLayout();
    VkDescriptorSet GetVKDescriptorSet();

    // request a pipeline object according to config
    // we may have slightly different pipelines with different configs, VKShader should cache these pipelines(TODO)
    VkPipeline RequestPipeline(const ShaderConfig& config, VkRenderPass renderPass, uint32_t subpass);
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
    typedef std::unordered_map<SetNum, std::vector<VkDescriptorSetLayoutBinding>> DescriptorSetBindings;
    typedef std::vector<std::unordered_map<VkDescriptorType, VkDescriptorPoolSize>> PoolSizeMap;
    struct PipelineCache
    {
        VkPipeline pipeline;
        VkRenderPass renderPass;
        uint32_t subpass;
        ShaderConfig config;
    };

    std::string name;
    ShaderInfo::ShaderInfo shaderInfo;
    VKObjectManager* objManager;
    UniPtr<VKShaderModule> vertShaderModule;
    UniPtr<VKShaderModule> fragShaderModule;
    std::unique_ptr<VKShaderModule> computeShaderModule;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VKSwapChain* swapchain;
    std::vector<PipelineCache> caches;
    std::vector<VkSampler> immutableSamplers;
    std::vector<size_t> layoutHash;
    std::vector<RefPtr<VKDescriptorPool>> descriptorPools;
    VkDescriptorSet descriptorSet;

    void GeneratePipelineLayoutAndGetDescriptorPool(DescriptorSetBindings& combined);

    std::shared_ptr<const ShaderConfig> defaultShaderConfig = {};
};
} // namespace Gfx
