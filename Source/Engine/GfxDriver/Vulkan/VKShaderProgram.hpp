#pragma once
#include "../ShaderProgram.hpp"
#include "VKDescriptorSetSlot.hpp"
#include "VKShaderInfo.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <unordered_map>
namespace Engine::Gfx
{
namespace Exp
{
    class VKShaderModule;
}
    class VKSwapChain;
    class VKObjectManager;
    class VKDescriptorPool;
    class VKShaderBufferStrategy;
    struct VKContext;
    struct ShaderPushConstant;
    class VKShaderProgram : public ShaderProgram
    {
        public:
            VKShaderProgram(const ShaderConfig* config,
                            RefPtr<VKContext> context,
                            const std::string& name, 
                            unsigned char* vert,
                            uint32_t vertSize,
                            unsigned char* frag,
                            uint32_t fragSize);
            VKShaderProgram(const VKShaderProgram& other) = delete;
            ~VKShaderProgram() override;

            VkPipelineLayout GetVKPipelineLayout();
            VkDescriptorSet GetVKDescriptorSet();

            // request a pipeline object according to config
            // we may have slightly different pipelines with different configs, VKShader should cache these pipelines(TODO)
            VkPipeline RequestPipeline(const ShaderConfig& config, VkRenderPass renderPass, uint32_t subpass);
            VKDescriptorPool& GetDescriptorPool(DescriptorSetSlot slot);

            const ShaderConfig& GetDefaultShaderConfig() override;

            const ShaderInfo::ShaderInfo& GetShaderInfo() { return shaderInfo; }
            const std::string& GetName() override {return name;}
        private:
            std::string name;
            ShaderInfo::ShaderInfo shaderInfo;

            typedef std::vector<std::vector<VkDescriptorSetLayoutBinding>> DescriptorSetBindings;
            typedef std::vector<std::unordered_map<VkDescriptorType, VkDescriptorPoolSize>> PoolSizeMap;

            VKObjectManager* objManager;
            UniPtr<Exp::VKShaderModule> vertShaderModule;
            UniPtr<Exp::VKShaderModule> fragShaderModule;
            VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
            VKSwapChain* swapchain;

            struct PipelineCache
            {
                VkPipeline pipeline;
                VkRenderPass renderPass;
                uint32_t subpass;
                ShaderConfig config;
            };
            std::vector<PipelineCache> caches;

            std::vector<RefPtr<VKDescriptorPool>> descriptorPools;
            VkDescriptorSet descriptorSet;

            void GeneratePipelineLayoutAndGetDescriptorPool(DescriptorSetBindings& combined);

            // TODO: This should be filled when shader loaded from shader files. Currently we don't have that functionality
            ShaderConfig defaultShaderConfig = {};
    };
}
