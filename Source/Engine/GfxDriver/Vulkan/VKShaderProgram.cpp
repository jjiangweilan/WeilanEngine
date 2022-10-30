#include "Code/Ptr.hpp"

#include "VKShaderProgram.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKShaderModule.hpp"
#include "Internal/VKSwapChain.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKDescriptorPool.hpp"
#include "VKContext.hpp"
#include <spdlog/spdlog.h>
#include <assert.h>


namespace Engine::Gfx
{
    VkStencilOpState MapVKStencilOpState(const StencilOpState& stencilOpState)
    {
        VkStencilOpState s;
        s.reference = stencilOpState.reference;
        s.writeMask = stencilOpState.writeMask;
        s.compareMask = stencilOpState.compareMask;
        s.compareOp = VKEnumMapper::MapCompareOp(stencilOpState.compareOp);
        s.depthFailOp = VKEnumMapper::MapStencilOp(stencilOpState.depthFailOp);
        s.failOp = VKEnumMapper::MapStencilOp(stencilOpState.failOp);
        s.passOp = VKEnumMapper::MapStencilOp(stencilOpState.passOp);

        return s;
    }

    VkPipelineColorBlendAttachmentState MapColorBlendAttachmentState(const ColorBlendAttachmentState& c)
    {
        VkPipelineColorBlendAttachmentState state;
        state.blendEnable = c.blendEnable;
        state.srcColorBlendFactor = VKEnumMapper::MapBlendFactor(c.srcColorBlendFactor);
        state.dstColorBlendFactor = VKEnumMapper::MapBlendFactor(c.dstColorBlendFactor);
        state.colorBlendOp = VKEnumMapper::MapBlendOp(c.colorBlendOp);
        state.srcAlphaBlendFactor = VKEnumMapper::MapBlendFactor(c.srcAlphaBlendFactor);
        state.dstAlphaBlendFactor = VKEnumMapper::MapBlendFactor(c.dstAlphaBlendFactor);
        state.alphaBlendOp = VKEnumMapper::MapBlendOp(c.alphaBlendOp);
        state.colorWriteMask = VKEnumMapper::MapColorComponentBits(c.colorWriteMask);

        return state;
    }

    VKDescriptorPool& VKShaderProgram::GetDescriptorPool(DescriptorSetSlot slot)
    {
        return *descriptorPools[slot];
    }

    VKShaderProgram::VKShaderProgram(const ShaderConfig* config, RefPtr<VKContext> context, const std::string& name,
            unsigned char* vertCode,
            uint32_t vertSize,
            unsigned char* fragCode,
            uint32_t fragSize) :
        name(name),
        objManager(context->objManager.Get()),
        swapchain(context->swapchain.Get())
    {
        bool vertInterleaved = true;
        if (config != nullptr) vertInterleaved = config->vertexInterleaved;


        vertShaderModule = Engine::MakeUnique<VKShaderModule>(name, vertCode, vertSize, vertInterleaved); // the Engine:: namespace is necessary to pass MSVC compilation
        fragShaderModule = Engine::MakeUnique<VKShaderModule>(name, fragCode, fragSize, vertInterleaved);

        // combine ShaderStageInfo into ShaderInfo
        ShaderInfo::Utils::Merge(shaderInfo, vertShaderModule->GetShaderInfo());
        ShaderInfo::Utils::Merge(shaderInfo, fragShaderModule->GetShaderInfo());

        // generate bindings
        DescriptorSetBindings descriptorSetBindings;
        descriptorSetBindings.resize(Descriptor_Set_Count);
        for(auto& iter : shaderInfo.bindings)
        {
            ShaderInfo::Binding& binding = iter.second;
            VkDescriptorSetLayoutBinding b;
            b.stageFlags = ShaderInfo::Utils::MapShaderStage(binding.stages);
            b.binding = binding.bindingNum;
            b.descriptorCount = binding.count;
            b.descriptorType = ShaderInfo::Utils::MapBindingType(binding.type);
            b.pImmutableSamplers = VK_NULL_HANDLE;
            descriptorSetBindings[iter.second.setNum].push_back(b);
        }

        GeneratePipelineLayoutAndGetDescriptorPool(descriptorSetBindings);

        if (config != nullptr)
        {
            defaultShaderConfig = *config;
        }
        else
        {
            // generate default shader config
            auto& outputs = fragShaderModule->GetShaderInfo().outputs;
            defaultShaderConfig.depth.writeEnable = true;
            defaultShaderConfig.depth.testEnable = true;
            defaultShaderConfig.depth.boundTestEnable = false;
            defaultShaderConfig.depth.compOp = CompareOp::Less_or_Equal;
            defaultShaderConfig.cullMode = CullMode::Back;
            defaultShaderConfig.stencil.testEnable = false;
            defaultShaderConfig.color.blendConstants[0] = 1;
            defaultShaderConfig.color.blendConstants[1] = 1;
            defaultShaderConfig.color.blendConstants[2] = 1;
            defaultShaderConfig.color.blendConstants[3] = 1;
            defaultShaderConfig.color.blends.resize(outputs.size());
            for(uint32_t i = 0; i < outputs.size(); ++i)
            {
                defaultShaderConfig.color.blends[i].blendEnable = false;
                defaultShaderConfig.color.blends[i].colorWriteMask = 
                    Gfx::ColorComponentBit::Component_R_Bit |
                    Gfx::ColorComponentBit::Component_G_Bit |
                    Gfx::ColorComponentBit::Component_B_Bit |
                    Gfx::ColorComponentBit::Component_A_Bit;
            }
        }
    }

    VKShaderProgram::~VKShaderProgram()
    {
        if (pipelineLayout)
        {
            objManager->DestroyPipelineLayout(pipelineLayout);
        }

        for(auto v : caches)
        {
            objManager->DestroyPipeline(v.pipeline);
        }
    }

    void VKShaderProgram::GeneratePipelineLayoutAndGetDescriptorPool(DescriptorSetBindings& combined)
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = VK_NULL_HANDLE;
        pipelineLayoutCreateInfo.flags = 0;

        std::vector<RefPtr<VKShaderModule>> modules;
        if (vertShaderModule != nullptr) modules.push_back(vertShaderModule);
        if (fragShaderModule != nullptr) modules.push_back(fragShaderModule);

        // we use fixed amount of descriptor set. They are grouped by update frequency
        pipelineLayoutCreateInfo.setLayoutCount = Descriptor_Set_Count;
        VkDescriptorSetLayout layouts[Descriptor_Set_Count];
        for(uint32_t i = 0; i < Descriptor_Set_Count; ++i)
        {
            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.pNext = VK_NULL_HANDLE;
            descriptorSetLayoutCreateInfo.flags = 0;;
            descriptorSetLayoutCreateInfo.bindingCount = combined[i].size();
            descriptorSetLayoutCreateInfo.pBindings = combined[i].data();
            
            auto& pool = VKContext::Instance()->descriptorPoolCache->RequestDescriptorPool(name, descriptorSetLayoutCreateInfo);
            descriptorPools.push_back(&pool);
            layouts[i] = pool.GetLayout();
        }
        pipelineLayoutCreateInfo.pSetLayouts = layouts;

        VkPushConstantRange ranges[32];
        uint32_t i = 0;
        uint32_t offset = 0;
        for(auto iter = shaderInfo.pushConstants.begin();
                iter != shaderInfo.pushConstants.end();
                iter++)
        {
            ranges[i].size = iter->second.data.size;
            ranges[i].offset = offset;
            ranges[i].stageFlags = ShaderInfo::Utils::MapShaderStage(iter->second.stages);
            offset += iter->second.data.size;
            i += 1;
            assert(i < 32);
        }
        pipelineLayoutCreateInfo.pushConstantRangeCount = i;
        pipelineLayoutCreateInfo.pPushConstantRanges = ranges;

        objManager->CreatePipelineLayout(pipelineLayoutCreateInfo, pipelineLayout);
    }

    VkPipelineLayout VKShaderProgram::GetVKPipelineLayout()
    {
        return pipelineLayout;
    }

    const ShaderConfig& VKShaderProgram::GetDefaultShaderConfig()
    {
        return defaultShaderConfig;
    }

    VkDescriptorSet VKShaderProgram::GetVKDescriptorSet()
    {
        assert(0 && "Not implmented");
        return 0;
    }

    VkPipeline VKShaderProgram::RequestPipeline(const ShaderConfig& config, VkRenderPass renderPass, uint32_t subpass)
    {
        for(auto& cache : caches)
        {
            if (cache.config == config &&
                    cache.subpass == subpass &&
                    cache.renderPass == renderPass
               ) return cache.pipeline;
        }

        VkGraphicsPipelineCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;
        createInfo.stageCount = 2;

        const ShaderModuleGraphicsPipelineCreateInfos& vertGPInfos = vertShaderModule->GetShaderModuleGraphicsPipelineCreateInfos();
        const ShaderModuleGraphicsPipelineCreateInfos& fragGPInfos = fragShaderModule->GetShaderModuleGraphicsPipelineCreateInfos();

        VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {vertGPInfos.pipelineShaderStageCreateInfo, fragGPInfos.pipelineShaderStageCreateInfo};
        createInfo.pStages = shaderStageCreateInfos;

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
        pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        pipelineInputAssemblyStateCreateInfo.pNext = VK_NULL_HANDLE;
        pipelineInputAssemblyStateCreateInfo.flags = 0;
        pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = false;
        createInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;

        createInfo.pVertexInputState = &vertGPInfos.pipelineVertexInputStateCreateInfo;

        // viewportState
        createInfo.pTessellationState = VK_NULL_HANDLE;
        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
        pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pipelineViewportStateCreateInfo.pNext = VK_NULL_HANDLE;
        pipelineViewportStateCreateInfo.flags = 0;
        pipelineViewportStateCreateInfo.viewportCount = 1;
        VkViewport viewPort{};
        viewPort.x = 0;
        viewPort.y = 0;
        viewPort.width = swapchain->GetSwapChainInfo().extent.width;
        viewPort.height = swapchain->GetSwapChainInfo().extent.height;
        viewPort.minDepth = 0;
        viewPort.maxDepth = 1;
        pipelineViewportStateCreateInfo.pViewports = &viewPort;
        createInfo.pViewportState = &pipelineViewportStateCreateInfo;

        VkRect2D scissor;
        pipelineViewportStateCreateInfo.scissorCount = 1;
        scissor.extent = {swapchain->GetSwapChainInfo().extent.width, swapchain->GetSwapChainInfo().extent.height};
        scissor.offset = {0, 0};
        pipelineViewportStateCreateInfo.pScissors = &scissor;

        // rasterizationStage
        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
        pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pipelineRasterizationStateCreateInfo.pNext = VK_NULL_HANDLE;
        pipelineRasterizationStateCreateInfo.flags = 0;
        pipelineRasterizationStateCreateInfo.depthClampEnable = false;
        pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = false;
        pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineRasterizationStateCreateInfo.cullMode = VKEnumMapper::MapCullMode(config.cullMode);
        pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipelineRasterizationStateCreateInfo.depthBiasEnable = false;
        pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0;
        pipelineRasterizationStateCreateInfo.depthBiasClamp = false;
        pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0;
        pipelineRasterizationStateCreateInfo.lineWidth = 1;
        createInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;

        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
        multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateCreateInfo.pNext = VK_NULL_HANDLE;
        multisampleStateCreateInfo.flags = 0;
        multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleStateCreateInfo.sampleShadingEnable = false;
        multisampleStateCreateInfo.minSampleShading = 0;
        multisampleStateCreateInfo.pSampleMask = VK_NULL_HANDLE;
        multisampleStateCreateInfo.alphaToCoverageEnable = false;
        multisampleStateCreateInfo.alphaToOneEnable = false;
        createInfo.pMultisampleState = &multisampleStateCreateInfo;

        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
        depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateCreateInfo.pNext = VK_NULL_HANDLE;
        depthStencilStateCreateInfo.flags = 0;
        depthStencilStateCreateInfo.depthTestEnable = config.depth.testEnable;
        depthStencilStateCreateInfo.depthWriteEnable = config.depth.writeEnable;
        depthStencilStateCreateInfo.depthCompareOp = VKEnumMapper::MapCompareOp(config.depth.compOp);
        depthStencilStateCreateInfo.depthBoundsTestEnable = config.depth.boundTestEnable;
        depthStencilStateCreateInfo.stencilTestEnable = config.stencil.testEnable;
        depthStencilStateCreateInfo.front = MapVKStencilOpState(config.stencil.front);
        depthStencilStateCreateInfo.back = MapVKStencilOpState(config.stencil.back);
        depthStencilStateCreateInfo.minDepthBounds = config.depth.minBounds;
        depthStencilStateCreateInfo.maxDepthBounds = config.depth.maxBounds;
        createInfo.pDepthStencilState = &depthStencilStateCreateInfo;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.pNext = VK_NULL_HANDLE;
        colorBlendStateCreateInfo.flags = 0;
        colorBlendStateCreateInfo.logicOpEnable = false;
        colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_AND;

        std::vector<VkPipelineColorBlendAttachmentState> blendStates(fragShaderModule->GetShaderInfo().outputs.size());
        
        for(uint32_t i = 0; i < fragShaderModule->GetShaderInfo().outputs.size(); ++i)
        {
            if (i < config.color.blends.size())
            {
                blendStates[i] = MapColorBlendAttachmentState(config.color.blends[i]);
            }
            else
            {
                blendStates[i].blendEnable = false;
                blendStates[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            }
        }
        colorBlendStateCreateInfo.attachmentCount = blendStates.size();
        colorBlendStateCreateInfo.pAttachments = blendStates.data();
        colorBlendStateCreateInfo.blendConstants[0] = config.color.blendConstants[0];
        colorBlendStateCreateInfo.blendConstants[1] = config.color.blendConstants[1];
        colorBlendStateCreateInfo.blendConstants[2] = config.color.blendConstants[2];
        colorBlendStateCreateInfo.blendConstants[3] = config.color.blendConstants[3];
        createInfo.pColorBlendState = &colorBlendStateCreateInfo;


        VkDynamicState dynamicState[]
        {
            //VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        // TODO: these dynamic states need to be handled, some of them are currently compared in ShaderConfig, we need to remove that
        // VK_DYNAMIC_STATE_LINE_WIDTH,
        // VK_DYNAMIC_STATE_DEPTH_BIAS,
        // VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        // VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        // VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        // VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        // VK_DYNAMIC_STATE_STENCIL_REFERENCE};
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.pNext = VK_NULL_HANDLE;
        dynamicStateCreateInfo.flags = 0;
        dynamicStateCreateInfo.dynamicStateCount = sizeof(dynamicState) / sizeof(VkDynamicState);
        dynamicStateCreateInfo.pDynamicStates = dynamicState;
        createInfo.pDynamicState = &dynamicStateCreateInfo;

        createInfo.layout = pipelineLayout;
        createInfo.renderPass = renderPass;
        createInfo.subpass = subpass;
        createInfo.basePipelineHandle = VK_NULL_HANDLE;
        createInfo.basePipelineIndex = 0;

        VkPipeline pipeline;
        objManager->CreateGraphicsPipeline(createInfo, pipeline);

        PipelineCache newCache;
        newCache.pipeline = pipeline;
        newCache.config = config;
        newCache.renderPass = renderPass;
        newCache.subpass = subpass;
        caches.push_back(newCache);

        return pipeline;
}
}
