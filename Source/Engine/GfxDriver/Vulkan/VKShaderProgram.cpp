#include "VKRenderPass.hpp"

#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "Libs/Allocator/GlobalTempAllocator.hpp"
#include "Libs/Assert.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKShaderProgram.hpp"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_hash.hpp>
namespace Gfx
{

VKShaderProgram::VKShaderProgram(VKContext* context, const PipelineCreateInfo& createInfo)
    : ShaderProgram(false), name(createInfo.pipelineInfo.name), objManager(context->objManager)
{
    pipelineInfo = createInfo.pipelineInfo;
    defaultPipelineConfig = createInfo.defaultConfig;

    if (!createInfo.vertSpv.empty() && !createInfo.fragSpv.empty())
    {
        isCompute = false;
        VkShaderModuleCreateInfo vertexModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .codeSize = createInfo.vertSpv.size(),
            .pCode = (uint32_t*)createInfo.vertSpv.data()
        };

        VkShaderModuleCreateInfo fragmentModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .codeSize = createInfo.fragSpv.size(),
            .pCode = (uint32_t*)createInfo.fragSpv.data()
        };
        objManager->CreateShaderModule(vertexModuleCreateInfo, vertexModule);
        objManager->CreateShaderModule(fragmentModuleCreateInfo, fragmentModule);
        GeneratePipelineLayout();

        // vertex inputs
        if (pipelineInfo.isVertexInterleaved)
        {
            uint32_t offset = 0;
            vertexAttributeDescriptions.reserve(pipelineInfo.vertexInputs.size());
            for (auto& vertexAttribute : pipelineInfo.vertexInputs)
            {
                VkVertexInputAttributeDescription attributeDesc;
                attributeDesc.location = vertexAttribute.location;
                attributeDesc.binding = 0;
                attributeDesc.format = Gfx::MapFormat(vertexAttribute.format);
                attributeDesc.offset = offset;

                vertexAttributeDescriptions.push_back(attributeDesc);
                offset += vertexAttribute.size;
            }

            VkVertexInputBindingDescription bindingDesc;
            bindingDesc.binding = 0;
            bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDesc.stride = offset; // offset becomes the total stride size
            vertexInputBindingDescriptions.push_back(bindingDesc);
        }
        else
        {
            if (!pipelineInfo.vertexInputs.empty())
            {
                auto& vertexAttribute = pipelineInfo.vertexInputs[0];
                VkVertexInputAttributeDescription attributeDesc;
                attributeDesc.location = vertexAttribute.location;
                attributeDesc.binding = 0;
                attributeDesc.format = Gfx::MapFormat(vertexAttribute.format);
                attributeDesc.offset = 0;
                vertexAttributeDescriptions.push_back(attributeDesc);

                VkVertexInputBindingDescription bindingDesc;
                bindingDesc.binding = 0;
                bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                bindingDesc.stride = vertexAttribute.size;
                vertexInputBindingDescriptions.push_back(bindingDesc);

                size_t attributeOffset = 0;
                for (int i = 1; i < pipelineInfo.vertexInputs.size(); ++i)
                {
                    auto& vertexAttribute = pipelineInfo.vertexInputs[i];
                    VkVertexInputAttributeDescription attributeDesc;
                    attributeDesc.location = vertexAttribute.location;
                    attributeDesc.binding = 1;
                    attributeDesc.format = Gfx::MapFormat(vertexAttribute.format);
                    attributeDesc.offset = attributeOffset;
                    vertexAttributeDescriptions.push_back(attributeDesc);
                    attributeOffset += vertexAttribute.size;
                }

                // attributeOffset == 0 means there is no attributes
                if (attributeOffset != 0)
                {
                    VkVertexInputBindingDescription attrBindingDesc;
                    attrBindingDesc.binding = 1;
                    attrBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                    attrBindingDesc.stride = attributeOffset;
                    vertexInputBindingDescriptions.push_back(attrBindingDesc);
                }
            }
        }
    }
    else if (!createInfo.computeSpv.empty())
    {
        isCompute = true;
        VkShaderModuleCreateInfo computeModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .codeSize = createInfo.computeSpv.size(),
            .pCode = (uint32_t*)createInfo.computeSpv.data()
        };
        objManager->CreateShaderModule(computeModuleCreateInfo, computeModule);

        VkPipelineShaderStageCreateInfo computePipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = computeModule,
            .pName = "main",
            .pSpecializationInfo = VK_NULL_HANDLE,
        };
        GeneratePipelineLayout();

        VkComputePipelineCreateInfo computePipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .stage = computePipelineShaderStageCreateInfo,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };

        VkPipeline pipeline;
        objManager->CreateComputePipeline(computePipelineCreateInfo, pipeline);
        caches[0] = pipeline;
    }
    else
        ASSERT(0 && "not possible to create empty shader");
}

VKShaderProgram::~VKShaderProgram()
{
    if (vertexModule)
        objManager->DestroyShaderModule(vertexModule);
    if (fragmentModule)
        objManager->DestroyShaderModule(fragmentModule);
    if (computeModule)
        objManager->DestroyShaderModule(computeModule);

    if (pipelineLayout)
        objManager->DestroyPipelineLayout(pipelineLayout);

    for (auto v : caches)
    {
        objManager->DestroyPipeline(v.second);
    }
}

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

VkSamplerCreateInfo SamplerCachePool::GenerateSamplerCreateInfo(const Gfx::PipelineInfo::SamplerConfig& samplerConfig)
{
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = VK_NULL_HANDLE;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = MapFilter(samplerConfig.magFilter);
    samplerCreateInfo.minFilter = MapFilter(samplerConfig.minFilter); // VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = MapSamplerAddressMode(samplerConfig.addressModeU);
    samplerCreateInfo.addressModeV = MapSamplerAddressMode(samplerConfig.addressModeV);
    samplerCreateInfo.addressModeW = MapSamplerAddressMode(samplerConfig.addressModeW);
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.anisotropyEnable = samplerConfig.anisotropic;
    samplerCreateInfo.maxAnisotropy = 0;
    samplerCreateInfo.compareEnable = samplerConfig.enbaleCompare;
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

void VKShaderProgram::GeneratePipelineLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = VK_NULL_HANDLE;
    pipelineLayoutCreateInfo.flags = 0;
    // prepare data
    using DescriptorSetLayoutBindingVector =
        std::vector<VkDescriptorSetLayoutBinding, GlobalTempAllocator<VkDescriptorSetLayoutBinding>>;
    std::vector<VkDescriptorSetLayout> layouts(pipelineInfo.descriptorSets.size());
    std::vector<DescriptorSetLayoutBindingVector> descriptorSetLayoutBindingVectors(pipelineInfo.descriptorSets.size()
    ); // an unique memory location is needed for each descriptorSetLayoutBindingVector because vulkan_hash uses the
       // memory address as hashing input
    const int MaxImmutableSamplerBindings = 512;
    VkSampler immutableSamplers[MaxImmutableSamplerBindings] = {};
    int immutableSamplerIndex = 0;

    for (uint32_t i = 0; i < layouts.size(); ++i)
    {
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.pNext = VK_NULL_HANDLE;
        descriptorSetLayoutCreateInfo.flags = 0;

        const auto& descriptorSetInfos = pipelineInfo.descriptorSets[i];

        auto& descriptorSetLayoutBindingVector = descriptorSetLayoutBindingVectors[i];
        descriptorSetLayoutBindingVector.resize(descriptorSetInfos.bindings.size());

        for (size_t bindingIndex = 0; bindingIndex < descriptorSetInfos.bindings.size(); ++bindingIndex)
        {
            const auto& binding = descriptorSetInfos.bindings[bindingIndex];

            int immutableSamplerOffset = -1;
            if (binding.descriptorType == DescriptorType::CombinedImageSampler)
            {
                const auto& samplerConfig = descriptorSetInfos.samplerConfigs[binding.samplerIndex];
                VkSamplerCreateInfo samplerCreateInfo = SamplerCachePool::GenerateSamplerCreateInfo(samplerConfig);
                VkSampler sampler = SamplerCachePool::RequestSampler(samplerCreateInfo);
                immutableSamplerOffset = immutableSamplerIndex;
                for (int count = 0; count < descriptorSetInfos.bindings[bindingIndex].descriptorCount &&
                                    immutableSamplerIndex < MaxImmutableSamplerBindings;
                     count++)
                {
                    immutableSamplers[immutableSamplerIndex] = sampler;
                    immutableSamplerIndex++;
                }
            }

            descriptorSetLayoutBindingVector[bindingIndex].binding =
                descriptorSetInfos.bindings[bindingIndex].bindingNum;
            descriptorSetLayoutBindingVector[bindingIndex].descriptorType =
                MapDescriptorType(descriptorSetInfos.bindings[bindingIndex].descriptorType);
            descriptorSetLayoutBindingVector[bindingIndex].descriptorCount =
                descriptorSetInfos.bindings[bindingIndex].descriptorCount;
            descriptorSetLayoutBindingVector[bindingIndex].stageFlags =
                MapShaderStages(descriptorSetInfos.bindings[bindingIndex].stages);
            descriptorSetLayoutBindingVector[bindingIndex].pImmutableSamplers =
                immutableSamplerOffset == -1 ? nullptr : &immutableSamplers[immutableSamplerOffset];
        }

        descriptorSetLayoutCreateInfo.bindingCount = descriptorSetLayoutBindingVector.size();
        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindingVector.data();

        auto& pool =
            VKContext::Instance()->descriptorPoolCache->RequestDescriptorPool(name, descriptorSetLayoutCreateInfo);
        descriptorPools.push_back(&pool);
        layouts[i] = pool.GetLayout();
    }

    ASSERT(immutableSamplerIndex <= MaxImmutableSamplerBindings);

    pipelineLayoutCreateInfo.setLayoutCount = layouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = layouts.data();

    VkPushConstantRange pushConstantRanges[32];
    uint32_t pushConstantIndex = 0;
    uint32_t offset = 0;
    for (auto& p : pipelineInfo.pushConstants)
    {
        pushConstantRanges[pushConstantIndex].size = p.size;
        pushConstantRanges[pushConstantIndex].offset = offset;
        pushConstantRanges[pushConstantIndex].stageFlags = MapShaderStages(p.stages);
        offset += p.size;
        pushConstantIndex += 1;
        ASSERT(pushConstantIndex < 32);
    }
    pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantIndex;
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges;

    objManager->CreatePipelineLayout(pipelineLayoutCreateInfo, pipelineLayout);
}

VkPipelineLayout VKShaderProgram::GetVKPipelineLayout()
{
    return pipelineLayout;
}

VkPipeline VKShaderProgram::RequestComputePipeline()
{
    if (isCompute)
    {
        return caches.begin()->second;
    }
    else
    {
        SPDLOG_ERROR("Requesting a non compute shader");
        return VK_NULL_HANDLE;
    }
}

VkPipeline VKShaderProgram::RequestGraphicsPipeline(
    const PipelineConfig& config, VKRenderPass* renderPass, uint32_t subpassIndex
)
{
    PipelineRequestHash requestHash = config.GetHash();
    HashCombine(requestHash, renderPass->GetHandle());
    HashCombine(requestHash, subpassIndex);

    auto cacheIter = caches.find(requestHash);
    if (cacheIter != caches.end())
    {
        return cacheIter->second;
    }

    VkGraphicsPipelineCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.stageCount = 2;
    VkPipelineShaderStageCreateInfo vertexPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexModule,
        .pName = "main",
        .pSpecializationInfo = VK_NULL_HANDLE,
    };
    VkPipelineShaderStageCreateInfo fragmentPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentModule,
        .pName = "main",
        .pSpecializationInfo = VK_NULL_HANDLE,
    };
    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {
        vertexPipelineShaderStageCreateInfo,
        fragmentPipelineShaderStageCreateInfo
    };

    createInfo.pStages = shaderStageCreateInfos;

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.pNext = VK_NULL_HANDLE;
    pipelineInputAssemblyStateCreateInfo.flags = 0;
    pipelineInputAssemblyStateCreateInfo.topology = MapPrimitiveTopology(config->topology);
    pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = false;
    createInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindingDescriptions.size()),
        .pVertexBindingDescriptions = vertexInputBindingDescriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };

    createInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;

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
    pipelineRasterizationStateCreateInfo.polygonMode = MapPolygonMode(config->polygonMode);
    pipelineRasterizationStateCreateInfo.cullMode = MapCullMode(config->cullMode);
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
    depthStencilStateCreateInfo.depthTestEnable = config->depth.testEnable;
    depthStencilStateCreateInfo.depthWriteEnable = config->depth.writeEnable;
    depthStencilStateCreateInfo.depthCompareOp = MapCompareOp(config->depth.compOp);
    depthStencilStateCreateInfo.depthBoundsTestEnable = config->depth.boundTestEnable;
    depthStencilStateCreateInfo.stencilTestEnable = config->stencil.testEnable;
    depthStencilStateCreateInfo.front = MapVKStencilOpState(config->stencil.front);
    depthStencilStateCreateInfo.back = MapVKStencilOpState(config->stencil.back);
    depthStencilStateCreateInfo.minDepthBounds = config->depth.minBounds;
    depthStencilStateCreateInfo.maxDepthBounds = config->depth.maxBounds;
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
    std::vector<VkPipelineColorBlendAttachmentState, GlobalTempAllocator<VkPipelineColorBlendAttachmentState>>
        blendStates(subpassSize);
    for (uint32_t i = 0; i < subpassSize; ++i)
    {
        if (i < config->color.blends.size())
        {
            blendStates[i] = MapColorBlendAttachmentState(config->color.blends[i]);
        }
        else
        {
            blendStates[i].blendEnable = false;
            if (i < pipelineInfo.fragmentOutputs.size())
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
    colorBlendStateCreateInfo.blendConstants[0] = config->color.blendConstants[0];
    colorBlendStateCreateInfo.blendConstants[1] = config->color.blendConstants[1];
    colorBlendStateCreateInfo.blendConstants[2] = config->color.blendConstants[2];
    colorBlendStateCreateInfo.blendConstants[3] = config->color.blendConstants[3];
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

    caches[requestHash] = pipeline;

    return pipeline;
}

} // namespace Gfx
