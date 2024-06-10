#include "VKObjectManager.hpp"
#include <spdlog/spdlog.h>

#define VK_CHECK(x)                                                                                                    \
    auto result = x;                                                                                                   \
    assert(result == VK_SUCCESS); // if (x != VK_SUCCESS) { SPDLOG_WARN("%s is not succeed", #x); }

#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif
namespace Gfx
{
VKObjectManager::VKObjectManager(VkDevice device) : device(device) {}

VKObjectManager::~VKObjectManager()
{
    DestroyPendingResources();
}

void VKObjectManager::CreateImageView(VkImageViewCreateInfo& createInfo, VkImageView& imageView)
{
    VK_CHECK(vkCreateImageView(device, &createInfo, VK_NULL_HANDLE, &imageView));
}

void VKObjectManager::DestroyImageView(VkImageView image)
{
    pendingImageViews.push_back(image);
}

void VKObjectManager::CreateRenderPass(VkRenderPassCreateInfo& createInfo, VkRenderPass& renderPass)
{
    VK_CHECK(vkCreateRenderPass(device, &createInfo, VK_NULL_HANDLE, &renderPass));
}

void VKObjectManager::DestroyRenderPass(VkRenderPass renderPass)
{
    pendingRenderPasses.push_back(renderPass);
}

void VKObjectManager::CreateFramebuffer(VkFramebufferCreateInfo& createInfo, VkFramebuffer& frameBuffer)
{
    VK_CHECK(vkCreateFramebuffer(device, &createInfo, VK_NULL_HANDLE, &frameBuffer));
}

void VKObjectManager::DestroyFramebuffer(VkFramebuffer frameBuffer)
{
    pendingFramebuffers.push_back(frameBuffer);
}

void VKObjectManager::CreateShaderModule(VkShaderModuleCreateInfo& createInfo, VkShaderModule& module)
{
    VK_CHECK(vkCreateShaderModule(device, &createInfo, VK_NULL_HANDLE, &module));
}

void VKObjectManager::DestroyShaderModule(VkShaderModule module)
{
    pendingShaderModules.push_back(module);
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
    pendingPipelines.push_back(pipeline);
}

void VKObjectManager::CreateDescriptorSetLayout(
    VkDescriptorSetLayoutCreateInfo& createInfo, VkDescriptorSetLayout& layout
)
{
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, VK_NULL_HANDLE, &layout));
}

void VKObjectManager::DestroyDescriptorSetLayout(VkDescriptorSetLayout layout)
{
    pendingDescriptorSetLayouts.push_back(layout);
}

void VKObjectManager::CreatePipelineLayout(VkPipelineLayoutCreateInfo& createInfo, VkPipelineLayout& layout)
{
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, VK_NULL_HANDLE, &layout));
}

void VKObjectManager::DestroyPipelineLayout(VkPipelineLayout layout)
{
    pendingPipelineLayout.push_back(layout);
}

void VKObjectManager::CreateDescriptorPool(VkDescriptorPoolCreateInfo& createInfo, VkDescriptorPool& pool)
{
    VK_CHECK(vkCreateDescriptorPool(device, &createInfo, VK_NULL_HANDLE, &pool));
}

void VKObjectManager::DestroyDescriptorPool(VkDescriptorPool pool)
{
    pendingDescriptorPools.push_back(pool);
}

void VKObjectManager::CreateSemaphore(VkSemaphoreCreateInfo& createInfo, VkSemaphore& semaphore)
{
    VK_CHECK(vkCreateSemaphore(device, &createInfo, VK_NULL_HANDLE, &semaphore));
}

void VKObjectManager::DestroySemaphore(VkSemaphore semaphore)
{
    pendingSemaphores.push_back(semaphore);
}

void VKObjectManager::CreateSampler(VkSamplerCreateInfo& createInfo, VkSampler& sampler)
{
    VK_CHECK(vkCreateSampler(device, &createInfo, VK_NULL_HANDLE, &sampler));
}
void VKObjectManager::DestroySampler(VkSampler sampler)
{
    pendingSamplers.push_back(sampler);
}

void VKObjectManager::DestroyCommandPool(VkCommandPool pool) {}

void VKObjectManager::DestroyPendingResources()
{
    for (auto v : pendingImageViews)
        vkDestroyImageView(device, v, VK_NULL_HANDLE);
    pendingImageViews.clear();

    for (auto v : pendingRenderPasses)
        vkDestroyRenderPass(device, v, VK_NULL_HANDLE);
    pendingRenderPasses.clear();

    for (auto v : pendingFramebuffers)
        vkDestroyFramebuffer(device, v, VK_NULL_HANDLE);
    pendingFramebuffers.clear();

    for (auto v : pendingShaderModules)
        vkDestroyShaderModule(device, v, VK_NULL_HANDLE);
    pendingShaderModules.clear();

    for (auto v : pendingPipelines)
        vkDestroyPipeline(device, v, VK_NULL_HANDLE);
    pendingPipelines.clear();

    for (auto v : pendingDescriptorSetLayouts)
        vkDestroyDescriptorSetLayout(device, v, VK_NULL_HANDLE);
    pendingDescriptorSetLayouts.clear();

    for (auto v : pendingPipelineLayout)
        vkDestroyPipelineLayout(device, v, VK_NULL_HANDLE);
    pendingPipelineLayout.clear();

    for (auto v : pendingDescriptorPools)
        vkDestroyDescriptorPool(device, v, VK_NULL_HANDLE);
    pendingDescriptorPools.clear();

    for (auto v : pendingSemaphores)
        vkDestroySemaphore(device, v, VK_NULL_HANDLE);
    pendingSemaphores.clear();

    for (auto v : pendingSamplers)
        vkDestroySampler(device, v, VK_NULL_HANDLE);
    pendingSamplers.clear();
}
} // namespace Gfx
