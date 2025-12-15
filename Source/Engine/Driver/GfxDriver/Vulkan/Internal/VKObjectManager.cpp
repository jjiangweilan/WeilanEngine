#include "VKObjectManager.hpp"
#include "Library/Assert.hpp"
#include <spdlog/spdlog.h>

#define VK_CHECK(x)                                                                                                    \
    auto result = x;                                                                                                   \
    ASSERT(result == VK_SUCCESS); // if (x != VK_SUCCESS) { SPDLOG_WARN("%s is not succeed", #x); }

#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif
namespace Gfx
{
VKObjectManager::VKObjectManager(VkDevice device) : device(device) {}

VKObjectManager::~VKObjectManager()
{
    DestroyPendingResources(true);
}

void VKObjectManager::CreateImageView(VkImageViewCreateInfo& createInfo, VkImageView& imageView)
{
    VK_CHECK(vkCreateImageView(device, &createInfo, VK_NULL_HANDLE, &imageView));
}

void VKObjectManager::DestroyImageView(VkImageView image)
{
    pendingImageViews.push_back({image, -1});
}

void VKObjectManager::CreateRenderPass(VkRenderPassCreateInfo& createInfo, VkRenderPass& renderPass)
{
    VK_CHECK(vkCreateRenderPass(device, &createInfo, VK_NULL_HANDLE, &renderPass));
}

void VKObjectManager::DestroyRenderPass(VkRenderPass renderPass)
{
    pendingRenderPasses.push_back({renderPass, -1});
}

void VKObjectManager::CreateFramebuffer(VkFramebufferCreateInfo& createInfo, VkFramebuffer& frameBuffer)
{
    VK_CHECK(vkCreateFramebuffer(device, &createInfo, VK_NULL_HANDLE, &frameBuffer));
}

void VKObjectManager::DestroyFramebuffer(VkFramebuffer frameBuffer)
{
    pendingFramebuffers.push_back({frameBuffer, -1});
}

void VKObjectManager::CreateShaderModule(VkShaderModuleCreateInfo& createInfo, VkShaderModule& module)
{
    VK_CHECK(vkCreateShaderModule(device, &createInfo, VK_NULL_HANDLE, &module));
}

void VKObjectManager::DestroyShaderModule(VkShaderModule module)
{
    pendingShaderModules.push_back({module, -1});
}

void VKObjectManager::CreateGraphicsPipeline(VkGraphicsPipelineCreateInfo& createInfo, VkPipeline& pipeline)
{
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, VK_NULL_HANDLE, &pipeline));
}

void VKObjectManager::CreateComputePipeline(VkComputePipelineCreateInfo& createInfo, VkPipeline& pipeline)
{
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, VK_NULL_HANDLE, &pipeline));
}

void VKObjectManager::DestroyPipeline(VkPipeline pipeline)
{
    pendingPipelines.push_back({pipeline, -1});
}

void VKObjectManager::CreateDescriptorSetLayout(
    VkDescriptorSetLayoutCreateInfo& createInfo, VkDescriptorSetLayout& layout
)
{
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, VK_NULL_HANDLE, &layout));
}

void VKObjectManager::DestroyDescriptorSetLayout(VkDescriptorSetLayout layout)
{
    pendingDescriptorSetLayouts.push_back({layout, -1});
}

void VKObjectManager::CreatePipelineLayout(VkPipelineLayoutCreateInfo& createInfo, VkPipelineLayout& layout)
{
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, VK_NULL_HANDLE, &layout));
}

void VKObjectManager::DestroyPipelineLayout(VkPipelineLayout layout)
{
    pendingPipelineLayout.push_back({layout, -1});
}

void VKObjectManager::CreateDescriptorPool(VkDescriptorPoolCreateInfo& createInfo, VkDescriptorPool& pool)
{
    VK_CHECK(vkCreateDescriptorPool(device, &createInfo, VK_NULL_HANDLE, &pool));
}

void VKObjectManager::DestroyDescriptorPool(VkDescriptorPool pool)
{
    pendingDescriptorPools.push_back({pool, -1});
}

void VKObjectManager::CreateSemaphore(VkSemaphoreCreateInfo& createInfo, VkSemaphore& semaphore)
{
    VK_CHECK(vkCreateSemaphore(device, &createInfo, VK_NULL_HANDLE, &semaphore));
}

void VKObjectManager::DestroySemaphore(VkSemaphore semaphore)
{
    pendingSemaphores.push_back({semaphore, -1});
}

void VKObjectManager::CreateSampler(VkSamplerCreateInfo& createInfo, VkSampler& sampler)
{
    VK_CHECK(vkCreateSampler(device, &createInfo, VK_NULL_HANDLE, &sampler));
}
void VKObjectManager::DestroySampler(VkSampler sampler)
{
    pendingSamplers.push_back({sampler, -1});
}

void VKObjectManager::DestroyCommandPool(VkCommandPool pool) {}

template <class T, class F>
void VKObjectManager::DestroyPendingResourcesOfType(std::list<Info>& resources, F f, bool destoryAll)
{
    for (auto curr = resources.begin(); curr != resources.end();)
    {
        if (curr->frameCount++ > 5 || destoryAll)
        {
            f(device, static_cast<T>(curr->ptr), VK_NULL_HANDLE);
            auto tmp = curr;
            curr++;
            resources.erase(tmp);
        }
        else
            curr++;
    }
}

void VKObjectManager::DestroyPendingResources(bool destroyAll)
{
    DestroyPendingResourcesOfType<VkImageView>(pendingImageViews, vkDestroyImageView, destroyAll);
    DestroyPendingResourcesOfType<VkRenderPass>(pendingRenderPasses, vkDestroyRenderPass, destroyAll);
    DestroyPendingResourcesOfType<VkFramebuffer>(pendingFramebuffers, vkDestroyFramebuffer, destroyAll);
    DestroyPendingResourcesOfType<VkShaderModule>(pendingShaderModules, vkDestroyShaderModule, destroyAll);
    DestroyPendingResourcesOfType<VkPipeline>(pendingPipelines, vkDestroyPipeline, destroyAll);
    DestroyPendingResourcesOfType<VkDescriptorSetLayout>(pendingDescriptorSetLayouts, vkDestroyDescriptorSetLayout, destroyAll);
    DestroyPendingResourcesOfType<VkPipelineLayout>(pendingPipelineLayout, vkDestroyPipelineLayout, destroyAll);
    DestroyPendingResourcesOfType<VkDescriptorPool>(pendingDescriptorPools, vkDestroyDescriptorPool, destroyAll);
    DestroyPendingResourcesOfType<VkSemaphore>(pendingSemaphores, vkDestroySemaphore, destroyAll);
    DestroyPendingResourcesOfType<VkSampler>(pendingSamplers, vkDestroySampler, destroyAll);
}
} // namespace Gfx
