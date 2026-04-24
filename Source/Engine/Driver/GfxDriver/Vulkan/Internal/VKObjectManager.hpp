#pragma once
#include <list>
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif

namespace Gfx
{
class VKObjectManager
{
public:
    VKObjectManager(VkDevice device);
    ~VKObjectManager();
    void CreateImageView(VkImageViewCreateInfo& createInfo, VkImageView& imageView);
    void DestroyImageView(VkImageView image);
    void CreateRenderPass(VkRenderPassCreateInfo& createInfo, VkRenderPass& renderPass);
    void DestroyRenderPass(VkRenderPass renderPass);
    void CreateFramebuffer(VkFramebufferCreateInfo& createInfo, VkFramebuffer& frameBuffer);
    void DestroyFramebuffer(VkFramebuffer frameBuffer);
    void CreateShaderModule(VkShaderModuleCreateInfo& createInfo, VkShaderModule& module);
    void DestroyShaderModule(VkShaderModule module);
    void CreateGraphicsPipeline(VkGraphicsPipelineCreateInfo& createInfo, VkPipeline& pipeline);
    void CreateComputePipeline(VkComputePipelineCreateInfo& createInfo, VkPipeline& pipeline);
    void DestroyPipeline(VkPipeline pipeline);
    void CreateDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo& createInfo, VkDescriptorSetLayout& layout);
    void DestroyDescriptorSetLayout(VkDescriptorSetLayout layout);
    void CreatePipelineLayout(VkPipelineLayoutCreateInfo& createInfo, VkPipelineLayout& layout);
    void DestroyPipelineLayout(VkPipelineLayout layout);
    void CreateDescriptorPool(VkDescriptorPoolCreateInfo& createInfo, VkDescriptorPool& pool);
    void DestroyDescriptorPool(VkDescriptorPool pool);
    void CreateSemaphore(VkSemaphoreCreateInfo& createInfo, VkSemaphore& semaphore);
    void DestroySemaphore(VkSemaphore semaphore);
    void CreateSampler(VkSamplerCreateInfo& createInfo, VkSampler& sampler);
    void DestroySampler(VkSampler sampler);

    void DestroyCommandPool(VkCommandPool pool);

    void DestroyPendingResources(bool destroyAll = false);

    VkDevice GetDevice()
    {
        return device;
    }

private:
    struct Info
    {
        void* ptr;
        int frameCount;
    };

    std::list<Info> pendingImageViews;
    std::list<Info> pendingRenderPasses;
    std::list<Info> pendingFramebuffers;
    std::list<Info> pendingShaderModules;
    std::list<Info> pendingPipelines;
    std::list<Info> pendingDescriptorSetLayouts;
    std::list<Info> pendingPipelineLayout;
    std::list<Info> pendingDescriptorPools;
    std::list<Info> pendingSemaphores;
    std::list<Info> pendingSamplers;
    std::list<Info> pendingCommandPools;
    VkDevice device;

    template <class T, class F>
    void DestroyPendingResourcesOfType(std::list<Info>& resources, F f, bool destoryAll);
};
} // namespace Gfx
