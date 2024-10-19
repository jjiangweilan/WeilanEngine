#include "VKRenderPass.hpp"

#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKShaderModule.hpp"
#include "VKShaderProgram.hpp"
#include <assert.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_hash.hpp>
namespace Gfx
{

VkSampler SamplerCachePool::RequestSampler(VkSamplerCreateInfo& createInfo)
{
    auto iter = samplers.find(createInfo);
    if (iter != samplers.end())
    {
        return iter->second;
    }

    VkSampler sampler;
    VKContext::Instance()->objManager->CreateSampler(createInfo, sampler);
    samplers[createInfo] = sampler;
    return sampler;
}

void SamplerCachePool::DestroyPool()
{
    for (auto& iter : samplers)
    {
        VKContext::Instance()->objManager->DestroySampler(iter.second);
    }

    samplers.clear();
}

std::unordered_map<vk::SamplerCreateInfo, VkSampler> SamplerCachePool::samplers =
    std::unordered_map<vk::SamplerCreateInfo, VkSampler>();

VkSamplerCreateInfo SamplerCachePool::GenerateSamplerCreateInfoFromString(
    const std::string& lowerBindingName, bool enableCompare
)
{
    VkFilter filter = VK_FILTER_LINEAR;
    // if (bindingName.find("linear")) filter = VK_FILTER_LINEAR;
    if (lowerBindingName.find("_point") != lowerBindingName.npos)
        filter = VK_FILTER_NEAREST;

    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    if (lowerBindingName.find("_clamptoborder") != lowerBindingName.npos)
        addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    else if (lowerBindingName.find("_clamp") != lowerBindingName.npos)
        addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkBool32 anisotropyEnable = VK_FALSE;
    if (lowerBindingName.find("_anisotropic") != lowerBindingName.npos)
        anisotropyEnable = VK_TRUE;

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = VK_NULL_HANDLE;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = filter; // VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = filter; // VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = addressMode;
    samplerCreateInfo.addressModeV = addressMode;
    samplerCreateInfo.addressModeW = addressMode;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.anisotropyEnable = anisotropyEnable;
    samplerCreateInfo.maxAnisotropy = 0;
    samplerCreateInfo.compareEnable = enableCompare;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    return samplerCreateInfo;
}

VkStencilOpState MapVKStencilOpState(const StencilOpState& stencilOpState)
{
    VkStencilOpState s;
    s.reference = stencilOpState.reference;
    s.writeMask = stencilOpState.writeMask;
    s.compareMask = stencilOpState.compareMask;
    s.compareOp = MapCompareOp(stencilOpState.compareOp);
    s.depthFailOp = MapStencilOp(stencilOpState.depthFailOp);
    s.failOp = MapStencilOp(stencilOpState.failOp);
    s.passOp = MapStencilOp(stencilOpState.passOp);

    return s;
}

VkPipelineColorBlendAttachmentState MapColorBlendAttachmentState(const ColorBlendAttachmentState& c)
{
    VkPipelineColorBlendAttachmentState state;
    state.blendEnable = c.blendEnable;
    state.srcColorBlendFactor = MapBlendFactor(c.srcColorBlendFactor);
    state.dstColorBlendFactor = MapBlendFactor(c.dstColorBlendFactor);
    state.colorBlendOp = MapBlendOp(c.colorBlendOp);
    state.srcAlphaBlendFactor = MapBlendFactor(c.srcAlphaBlendFactor);
    state.dstAlphaBlendFactor = MapBlendFactor(c.dstAlphaBlendFactor);
    state.alphaBlendOp = MapBlendOp(c.alphaBlendOp);
    state.colorWriteMask = MapColorComponentBits(c.colorWriteMask);

    return state;
}

VKDescriptorPool& VKShaderProgram::GetDescriptorPool(DescriptorSetSlot slot)
{
    return *descriptorPools[slot];
}

VKShaderProgram::VKShaderProgram(
    std::shared_ptr<const ShaderConfig> config,
    VKContext* context,
    const std::string& name,
    ShaderProgramCreateInfo& createInfo
)
    : ShaderProgram(createInfo.compSpv.size() != 0), name(name), objManager(context->objManager)
{
    // compile as a compute shader
    if (createInfo.compSpv.size() != 0)
    {
        computeShaderModule =
            std::make_unique<VKShaderModule>(name, createInfo.compSpv, createInfo.compReflection, false, *config);

        ShaderInfo::Utils::Merge(shaderInfo, computeShaderModule->GetShaderInfo());
        CreateShaderPipeline(config, computeShaderModule.get());
    }
    else
    {
        bool vertInterleaved = true;
        if (config != nullptr)
            vertInterleaved = config->vertexInterleaved;

        vertShaderModule = MakeUnique<VKShaderModule>(
            name,
            createInfo.vertSpv,
            createInfo.vertReflection,
            vertInterleaved,
            *config
        ); // the  namespace is necessary to pass MSVC compilation
        fragShaderModule =
            MakeUnique<VKShaderModule>(name, createInfo.fragSpv, createInfo.fragReflection, vertInterleaved, *config);

        // combine ShaderStageInfo into ShaderInfo
        ShaderInfo::Utils::Merge(shaderInfo, vertShaderModule->GetShaderInfo());
        ShaderInfo::Utils::Merge(shaderInfo, fragShaderModule->GetShaderInfo());

        CreateShaderPipeline(config, fragShaderModule.get());
    }
}

VKShaderProgram::~VKShaderProgram()
{
    if (pipelineLayout)
    {
        objManager->DestroyPipelineLayout(pipelineLayout);
    }

    for (auto v : caches)
    {
        objManager->DestroyPipeline(v.pipeline);
    }
}

void VKShaderProgram::CreateShaderPipeline(
    std::shared_ptr<const ShaderConfig> config, VKShaderModule* fallbackConfigModule
)
{

    for (auto& b : shaderInfo.bindings)
    {
        shaderInfo.descriptorSetBindingMap[b.second.setNum].push_back(&b.second);
    }

    // generate bindings
    for (auto& iter : shaderInfo.bindings)
    {
        auto& descriptorSetWrap = descriptorSetBindings[iter.second.setNum];
        ShaderInfo::Binding& binding = iter.second;

        auto bindingIter = std::find_if(
            descriptorSetWrap.binding.begin(),
            descriptorSetWrap.binding.end(),
            [&binding](VkDescriptorSetLayoutBinding& b) { return b.binding == binding.bindingNum; }
        );

        if (bindingIter != descriptorSetWrap.binding.end())
        {
            continue;
        }

        VkDescriptorSetLayoutBinding b{};
        b.stageFlags = ShaderInfo::Utils::MapShaderStage(binding.stages);
        b.binding = binding.bindingNum;
        b.descriptorCount = binding.count;
        b.descriptorType = ShaderInfo::Utils::MapBindingType(binding.type);

        if (binding.type == ShaderInfo::BindingType::Texture)
        {
            std::string lowerBindingName = iter.first;
            for (auto& c : lowerBindingName)
            {
                c = std::tolower(c);
            }
            VkSamplerCreateInfo createInfo = SamplerCachePool::GenerateSamplerCreateInfoFromString(
                lowerBindingName,
                binding.binding.texture.enableCompare
            );
            VkSampler sampler = SamplerCachePool::RequestSampler(createInfo);

            descriptorSetWrap.samplers.push_back(std::vector<VkSampler>(b.descriptorCount, sampler));
        }
        else
        {
            descriptorSetWrap.samplers.push_back(std::vector<VkSampler>());
        }

        descriptorSetWrap.binding.push_back(b);
    }

    for (auto& set : descriptorSetBindings)
    {
        for (int i = 0; i < set.second.binding.size(); i++)
        {
            if (set.second.binding[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                set.second.binding[i].pImmutableSamplers = set.second.samplers[i].data();
            else
                set.second.binding[i].pImmutableSamplers = nullptr;
        }
    }

    GeneratePipelineLayoutAndGetDescriptorPool(descriptorSetBindings);

    if (config != nullptr)
    {
        defaultShaderConfig = config;
    }
    else
    {
        ShaderConfig defaultShaderConfig;
        // generate default shader config
        auto& outputs = fallbackConfigModule->GetShaderInfo().outputs;
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
        for (uint32_t i = 0; i < outputs.size(); ++i)
        {
            defaultShaderConfig.color.blends[i].blendEnable = false;
            defaultShaderConfig.color.blends[i].colorWriteMask =
                Gfx::ColorComponentBit::Component_R_Bit | Gfx::ColorComponentBit::Component_G_Bit |
                Gfx::ColorComponentBit::Component_B_Bit | Gfx::ColorComponentBit::Component_A_Bit;
        }

        this->defaultShaderConfig = std::make_unique<ShaderConfig>(defaultShaderConfig);
    }
}

void VKShaderProgram::GeneratePipelineLayoutAndGetDescriptorPool(DescriptorSetBindings& combined)
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = VK_NULL_HANDLE;
    pipelineLayoutCreateInfo.flags = 0;

    std::vector<RefPtr<VKShaderModule>> modules;
    if (vertShaderModule != nullptr)
        modules.push_back(vertShaderModule);
    if (fragShaderModule != nullptr)
        modules.push_back(fragShaderModule);
    if (computeShaderModule != nullptr)
        modules.push_back(computeShaderModule);

    // we use fixed amount of descriptor set. They are grouped by update frequency. TODO: this is a very hard coded
    // solution. Need to change
    uint32_t maxSet = 0;
    for (auto& set : combined)
    {
        maxSet = set.first > maxSet ? set.first : maxSet;
    }
    uint32_t setCount = maxSet + 1;
    pipelineLayoutCreateInfo.setLayoutCount = setCount;
    std::vector<VkDescriptorSetLayout> layouts(setCount);
    for (uint32_t i = 0; i < setCount; ++i)
    {
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.pNext = VK_NULL_HANDLE;
        if (name == "ImGui")
        {
            descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        }
        else
        {
            descriptorSetLayoutCreateInfo.flags = 0;
        }
        descriptorSetLayoutCreateInfo.bindingCount = combined[i].binding.size();
        descriptorSetLayoutCreateInfo.pBindings = combined[i].binding.data();

        if ((descriptorSetLayoutCreateInfo.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0)
        {
            layoutHash.push_back(std::hash<vk::DescriptorSetLayoutCreateInfo>{}(descriptorSetLayoutCreateInfo));
        }
        auto& pool =
            VKContext::Instance()->descriptorPoolCache->RequestDescriptorPool(name, descriptorSetLayoutCreateInfo);
        descriptorPools.push_back(&pool);
        layouts[i] = pool.GetLayout();
    }
    pipelineLayoutCreateInfo.pSetLayouts = layouts.data();

    VkPushConstantRange ranges[32];
    uint32_t i = 0;
    uint32_t offset = 0;
    for (auto iter = shaderInfo.pushConstants.begin(); iter != shaderInfo.pushConstants.end(); iter++)
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
    return *defaultShaderConfig;
}

VkPipeline VKShaderProgram::RequestComputePipeline(const ShaderConfig& config)
{
    if (isCompute)
    {
        if (!caches.empty())
            return caches.front().pipeline;

        VkComputePipelineCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .stage = computeShaderModule->GetShaderModuleGraphicsPipelineCreateInfos().pipelineShaderStageCreateInfo,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };

        VkPipeline pipeline;
        objManager->CreateComputePipeline(createInfo, pipeline);

        PipelineCache newCache;
        newCache.pipeline = pipeline;
        newCache.config = config;
        caches.push_back(newCache);

        return pipeline;
    }
    else
    {
        SPDLOG_ERROR("Requesting a non compute shader");
        return VK_NULL_HANDLE;
    }
}

VkPipeline VKShaderProgram::RequestGraphicsPipeline(
    const ShaderConfig& config, VKRenderPass* renderPass, uint32_t subpassIndex
)
{
    for (auto& cache : caches)
    {
        if (cache.config == config && cache.subpass == subpassIndex && cache.renderPass == renderPass->GetHandle())
            return cache.pipeline;
    }

    VkGraphicsPipelineCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.stageCount = 2;

    const ShaderModuleGraphicsPipelineCreateInfos& vertGPInfos =
        vertShaderModule->GetShaderModuleGraphicsPipelineCreateInfos();
    const ShaderModuleGraphicsPipelineCreateInfos& fragGPInfos =
        fragShaderModule->GetShaderModuleGraphicsPipelineCreateInfos();

    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
        vertGPInfos.pipelineShaderStageCreateInfo,
        fragGPInfos.pipelineShaderStageCreateInfo
    };
    createInfo.pStages = shaderStageCreateInfos;

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.pNext = VK_NULL_HANDLE;
    pipelineInputAssemblyStateCreateInfo.flags = 0;
    pipelineInputAssemblyStateCreateInfo.topology = MapPrimitiveTopology(config.topology);
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
    viewPort.width = GetSwapchain()->extent.width;
    viewPort.height = GetSwapchain()->extent.height;
    viewPort.minDepth = 0;
    viewPort.maxDepth = 1;
    pipelineViewportStateCreateInfo.pViewports = &viewPort;
    createInfo.pViewportState = &pipelineViewportStateCreateInfo;

    VkRect2D scissor;
    pipelineViewportStateCreateInfo.scissorCount = 1;
    scissor.extent = {GetSwapchain()->extent.width, GetSwapchain()->extent.height};
    scissor.offset = {0, 0};
    pipelineViewportStateCreateInfo.pScissors = &scissor;

    // rasterizationStage
    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.pNext = VK_NULL_HANDLE;
    pipelineRasterizationStateCreateInfo.flags = 0;
    pipelineRasterizationStateCreateInfo.depthClampEnable = false;
    pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = false;
    pipelineRasterizationStateCreateInfo.polygonMode = MapPolygonMode(config.polygonMode);
    pipelineRasterizationStateCreateInfo.cullMode = MapCullMode(config.cullMode);
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
    depthStencilStateCreateInfo.depthCompareOp = MapCompareOp(config.depth.compOp);
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

    // protect unwritten output with color mask
    auto& subpass = renderPass->GetSubpesses()[subpassIndex];
    size_t subpassSize = subpass.colors.size();
    std::vector<VkPipelineColorBlendAttachmentState> blendStates(subpassSize);
    for (uint32_t i = 0; i < subpassSize; ++i)
    {
        if (i < config.color.blends.size())
        {
            blendStates[i] = MapColorBlendAttachmentState(config.color.blends[i]);
        }
        else
        {
            blendStates[i].blendEnable = false;
            if (i < fragShaderModule->GetShaderInfo().outputs.size())
            {
                blendStates[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            }
            else
            {
                blendStates[i].colorWriteMask = 0;
            }
        }
    }
    colorBlendStateCreateInfo.attachmentCount = blendStates.size();
    colorBlendStateCreateInfo.pAttachments = blendStates.data();
    colorBlendStateCreateInfo.blendConstants[0] = config.color.blendConstants[0];
    colorBlendStateCreateInfo.blendConstants[1] = config.color.blendConstants[1];
    colorBlendStateCreateInfo.blendConstants[2] = config.color.blendConstants[2];
    colorBlendStateCreateInfo.blendConstants[3] = config.color.blendConstants[3];
    createInfo.pColorBlendState = &colorBlendStateCreateInfo;

    VkDynamicState dynamicState[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    // TODO: these dynamic states need to be handled, some of them are currently compared in ShaderConfig, we need to
    // remove that VK_DYNAMIC_STATE_LINE_WIDTH, VK_DYNAMIC_STATE_DEPTH_BIAS, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
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
    createInfo.renderPass = renderPass->GetHandle();
    createInfo.subpass = subpassIndex;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;

    VkPipeline pipeline;
    objManager->CreateGraphicsPipeline(createInfo, pipeline);

    PipelineCache newCache;
    newCache.pipeline = pipeline;
    newCache.config = config;
    newCache.renderPass = renderPass->GetHandle();
    newCache.subpass = subpassIndex;
    caches.push_back(newCache);

    return pipeline;
}

size_t VKShaderProgram::GetLayoutHash(uint32_t set)
{
    if (set < layoutHash.size())
    {
        return layoutHash[set];
    }
    return 0;
}
} // namespace Gfx
