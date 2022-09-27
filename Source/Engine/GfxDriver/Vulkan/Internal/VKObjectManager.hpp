#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif

namespace Engine::Gfx
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

            void DestroyPendingResources();

            VkDevice GetDevice() {return device;}
        private:

            std::vector<VkImageView> pendingImageViews;
            std::vector<VkRenderPass> pendingRenderPasses;
            std::vector<VkFramebuffer> pendingFramebuffers;
            std::vector<VkShaderModule> pendingShaderModules;
            std::vector<VkPipeline> pendingPipelines;
            std::vector<VkDescriptorSetLayout> pendingDescriptorSetLayouts;
            std::vector<VkPipelineLayout> pendingPipelineLayout;
            std::vector<VkDescriptorPool> pendingDescriptorPools;
            std::vector<VkSemaphore> pendingSemaphores;
            std::vector<VkSampler> pendingSamplers;
            VkDevice device;
    };
}