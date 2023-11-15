#include "Libs/Ptr.hpp"

#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKShaderModule.hpp"
#include "VKShaderProgram.hpp"
#include <algorithm>
#include <assert.h>
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{

VkSampler SamplerCachePool::RequestSampler(VkSamplerCreateInfo& createInfo)
{
    uint32_t hash = XXH32(&createInfo, sizeof(VkSamplerCreateInfo), 0);

    auto iter = samplers.find(hash);
    if (iter != samplers.end())
    {
        return iter->second;
    }

    VkSampler sampler;
    VKContext::Instance()->objManager->CreateSampler(createInfo, sampler);
    samplers[hash] = sampler;
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

std::unordered_map<uint32_t, VkSampler> SamplerCachePool::samplers = std::unordered_map<uint32_t, VkSampler>();

VkSamplerCreateInfo SamplerCachePool::GenerateSamplerCreateInfoFromString(
    const std::string& lowerBindingName, bool enableCompare
)
{
    VkFilter filter = VK_FILTER_LINEAR;
    // if (bindingName.find("linear")) filter = VK_FILTER_LINEAR;
    if (lowerBindingName.find("_point") != lowerBindingName.npos)
        filter = VK_FILTER_NEAREST;

    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if (lowerBindingName.find("_clamp") != lowerBindingName.npos)
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
    RefPtr<VKContext> context,
    const std::string& name,
    const std::vector<uint32_t>& vert,
    const std::vector<uint32_t>& frag
)
    : VKShaderProgram(
          config,
          context,
          name,
          (const unsigned char*)&vert[0],
          vert.size() * sizeof(uint32_t),
          (const unsigned char*)&frag[0],
          frag.size() * sizeof(uint32_t)
      )
{}
VKShaderProgram::VKShaderProgram(
    std::shared_ptr<const ShaderConfig> config,
    VKContext* context,
    const std::string& name,
    const unsigned char* compute,
    uint32_t computeSize
)
    : name(name), objManager(context->objManager.Get()), swapchain(context->swapchain.Get())
{
    computeShaderModule = std::make_unique<VKShaderModule>(name, compute, computeSize, false);
}

VKShaderProgram::VKShaderProgram(
    std::shared_ptr<const ShaderConfig> config,
    RefPtr<VKContext> context,
    const std::string& name,
    const std::vector<uint32_t>& comp
)
    : VKShaderProgram(config, context.Get(), name, (const unsigned char*)&comp[0], comp.size() * sizeof(uint32_t))
{}

VKShaderProgram::VKShaderProgram(
    std::shared_ptr<const ShaderConfig> config,
    RefPtr<VKContext> context,
    const std::string& name,
    const unsigned char* vertCode,
    uint32_t vertSize,
    const unsigned char* fragCode,
    uint32_t fragSize
)
    : name(name), objManager(context->objManager.Get()), swapchain(context->swapchain.Get())
{
    bool vertInterleaved = true;
    if (config != nullptr)
        vertInterleaved = config->vertexInterleaved;

    vertShaderModule = MakeUnique<VKShaderModule>(
        name,
        vertCode,
        vertSize,
        vertInterleaved
    ); // the Engine:: namespace is necessary to pass MSVC compilation
    fragShaderModule = MakeUnique<VKShaderModule>(name, fragCode, fragSize, vertInterleaved);

    // combine ShaderStageInfo into ShaderInfo
    ShaderInfo::Utils::Merge(shaderInfo, vertShaderModule->GetShaderInfo());
    ShaderInfo::Utils::Merge(shaderInfo, fragShaderModule->GetShaderInfo());

    // generate bindings
    DescriptorSetBindings descriptorSetBindings;
    std::vector<VkSampler> immutableSamplerHandles;
    for (auto& iter : shaderInfo.bindings)
    {
        ShaderInfo::Binding& binding = iter.second;
        VkDescriptorSetLayoutBinding b;
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

            std::vector<VkSampler> samplerHandles(b.descriptorCount, sampler);
            int index = immutableSamplerHandles.size();
            immutableSamplerHandles.insert(immutableSamplerHandles.end(), samplerHandles.begin(), samplerHandles.end());
            b.pImmutableSamplers = reinterpret_cast<VkSampler*>(index + 1);
        }
        else
            b.pImmutableSamplers = VK_NULL_HANDLE;
        descriptorSetBindings[iter.second.setNum].push_back(b);
    }

    for (auto& set : descriptorSetBindings)
    {
        for (auto& binding : set.second)
        {
            if (binding.pImmutableSamplers != VK_NULL_HANDLE)
            {
                binding.pImmutableSamplers =
                    &immutableSamplerHandles[reinterpret_cast<uint64_t>(binding.pImmutableSamplers) - 1];
            }
        }

        // std::sort(
        //     set.second.begin(),
        //     set.second.end(),
        //     [](const VkDescriptorSetLayoutBinding& l, const VkDescriptorSetLayoutBinding& r)
        //     { return l.binding < r.binding; }
        //);
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
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
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
        descriptorSetLayoutCreateInfo.bindingCount = combined[i].size();
        descriptorSetLayoutCreateInfo.pBindings = combined[i].data();

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

VkDescriptorSet VKShaderProgram::GetVKDescriptorSet()
{
    assert(0 && "Not implmented");
    return 0;
}

VkPipeline VKShaderProgram::RequestPipeline(const ShaderConfig& config, VkRenderPass renderPass, uint32_t subpass)
{
    for (auto& cache : caches)
    {
        if (cache.config == config && cache.subpass == subpass && cache.renderPass == renderPass)
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
        fragGPInfos.pipelineShaderStageCreateInfo};
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

    std::vector<VkPipelineColorBlendAttachmentState> blendStates(fragShaderModule->GetShaderInfo().outputs.size());

    for (uint32_t i = 0; i < fragShaderModule->GetShaderInfo().outputs.size(); ++i)
    {
        if (i < config.color.blends.size())
        {
            blendStates[i] = MapColorBlendAttachmentState(config.color.blends[i]);
        }
        else
        {
            blendStates[i].blendEnable = false;
            blendStates[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
} // namespace Engine::Gfx
