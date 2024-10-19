#include "VKShaderModule.hpp"
#include "Libs/Ptr.hpp"
#include "VKContext.hpp"
#include <spdlog/spdlog.h>
#include <spirv_cross/spirv_reflect.hpp>

using namespace Gfx::ShaderInfo;

namespace Gfx
{

VkShaderStageFlagBits MapStage(const std::string& str)
{
    if (str == "vert")
        return VK_SHADER_STAGE_VERTEX_BIT;
    else if (str == "frag")
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    else if (str == "comp")
        return VK_SHADER_STAGE_COMPUTE_BIT;

    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

uint32_t MapTypeToSize(const std::string& str)
{
    if (str == "vec4")
        return sizeof(float) * 4;
    else if (str == "vec3")
        return sizeof(float) * 3;
    else if (str == "vec2")
        return sizeof(float) * 2;
    else if (str == "mat2")
        return sizeof(float) * 4;
    else if (str == "mat3")
        return sizeof(float) * 9;
    else if (str == "mat4")
        return sizeof(float) * 16;
    else if (str == "float")
        return sizeof(float);

    SPDLOG_ERROR("Failed to map stride");
    return 0;
}

std::string str_tolower(std::string s)
{
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) { return std::tolower(c); } // correct
    );
    return s;
}

VkFormat MapFormat(const std::string& str, const std::string& name)
{
    std::string lowerName = str_tolower(name);
    if (str == "vec4")
    {
        if (lowerName.find("color") != lowerName.npos)
        {
            if (lowerName.find("16") != lowerName.npos)
                return VK_FORMAT_R16G16B16A16_UNORM;
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    else if (str == "vec3")
    {
        if (lowerName.find("color") != lowerName.npos)
        {
            if (lowerName.find("16") != lowerName.npos)
                return VK_FORMAT_R16G16B16_UNORM;
            return VK_FORMAT_R8G8B8_UNORM;
        }
        return VK_FORMAT_R32G32B32_SFLOAT;
    }
    else if (str == "vec2")
    {
        if (lowerName.find("color") != lowerName.npos)
        {
            if (lowerName.find("16") != lowerName.npos)
                return VK_FORMAT_R16G16_UNORM;
            return VK_FORMAT_R8G8_UNORM;
        }
        return VK_FORMAT_R32G32_SFLOAT;
    }
    else if (str == "float")
    {
        if (lowerName.find("color") != lowerName.npos)
        {
            if (lowerName.find("16") != lowerName.npos)
                return VK_FORMAT_R16_UNORM;
            return VK_FORMAT_R8_UNORM;
        }
        return VK_FORMAT_R32_SFLOAT;
    }

    assert(0 && "Failed to map format");
    return (VkFormat)0;
}

VKShaderModule::VKShaderModule(
    const std::string& name,
    std::vector<uint32_t>& spv,
    nlohmann::json& reflection,
    bool vertInterleaved,
    const ShaderConfig& config
)
    : vertInterleaved(vertInterleaved), gpuProperties(GetGPU()->physicalDeviceProperties)
{
    nlohmann::json& jsonInfo = reflection;

    // Create shader modules
    if (jsonInfo["entryPoints"].size() > 1)
        SPDLOG_ERROR("Multiple entry points is not supported");
    auto& entry = jsonInfo["entryPoints"][0];
    stage = MapStage(entry["mode"]);
    entryPoint = entry["name"];
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.codeSize = spv.size() * sizeof(uint32_t);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());
    VKContext::Instance()->objManager->CreateShaderModule(createInfo, shaderModule);

    jsonInfo["spvPath"] = name;
    // Create shader info
    ShaderInfo::Utils::Process(shaderInfo, jsonInfo, config);

    gpCreateInfos = GenerateShaderModulePipelineCreateInfos();
}

VKShaderModule::~VKShaderModule()
{
    VKContext::Instance()->objManager->DestroyShaderModule(shaderModule);
}

const ShaderModuleGraphicsPipelineCreateInfos& VKShaderModule::GetShaderModuleGraphicsPipelineCreateInfos()
{
    return gpCreateInfos;
}

ShaderModuleGraphicsPipelineCreateInfos VKShaderModule::GenerateShaderModulePipelineCreateInfos()
{
    ShaderModuleGraphicsPipelineCreateInfos pipelineCreateInfo{};

    pipelineCreateInfo.pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineCreateInfo.pipelineShaderStageCreateInfo.pNext = VK_NULL_HANDLE;
    pipelineCreateInfo.pipelineShaderStageCreateInfo.flags = 0;
    pipelineCreateInfo.pipelineShaderStageCreateInfo.stage = stage;
    pipelineCreateInfo.pipelineShaderStageCreateInfo.module = shaderModule;
    pipelineCreateInfo.pipelineShaderStageCreateInfo.pName = "main";
    pipelineCreateInfo.pipelineShaderStageCreateInfo.pSpecializationInfo = VK_NULL_HANDLE;

    if (stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        assert(
            shaderInfo.inputs.size() < gpuProperties.limits.maxVertexInputBindings &&
            "input attributes exceed binding limits"
        );

        if (vertInterleaved)
        {
            uint32_t offset = 0;
            pipelineCreateInfo.vertexAttributeDescriptions.reserve(shaderInfo.inputs.size());
            for (auto& vertexAttribute : shaderInfo.inputs)
            {
                VkVertexInputAttributeDescription attributeDesc;
                attributeDesc.location = vertexAttribute.location;
                attributeDesc.binding = 0;
                attributeDesc.format = MapFormat(vertexAttribute.data.name, vertexAttribute.name);
                attributeDesc.offset = offset;
                pipelineCreateInfo.vertexAttributeDescriptions.push_back(attributeDesc);
                offset += vertexAttribute.data.size;
            }

            VkVertexInputBindingDescription bindingDesc;
            bindingDesc.binding = 0;
            bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDesc.stride = offset; // offset becomes the total stride size
            pipelineCreateInfo.vertexInputBindingDescriptions.push_back(bindingDesc);
        }
        else
        {
            if (!shaderInfo.inputs.empty())
            {
                auto& vertexAttribute = shaderInfo.inputs[0];
                VkVertexInputAttributeDescription attributeDesc;
                attributeDesc.location = vertexAttribute.location;
                attributeDesc.binding = 0;
                attributeDesc.format = MapFormat(vertexAttribute.data.name, vertexAttribute.name);
                attributeDesc.offset = 0; // vertexAttribute.offset;
                pipelineCreateInfo.vertexAttributeDescriptions.push_back(attributeDesc);

                VkVertexInputBindingDescription bindingDesc;
                bindingDesc.binding = 0;
                bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                bindingDesc.stride = vertexAttribute.data.size;
                pipelineCreateInfo.vertexInputBindingDescriptions.push_back(bindingDesc);

                size_t attributeOffset = 0;
                for (int i = 1; i < shaderInfo.inputs.size(); ++i)
                {
                    auto& vertexAttribute = shaderInfo.inputs[i];
                    VkVertexInputAttributeDescription attributeDesc;
                    attributeDesc.location = vertexAttribute.location;
                    attributeDesc.binding = 1;
                    attributeDesc.format = MapFormat(vertexAttribute.data.name, vertexAttribute.name);
                    attributeDesc.offset = attributeOffset;
                    pipelineCreateInfo.vertexAttributeDescriptions.push_back(attributeDesc);
                    attributeOffset += vertexAttribute.data.size;
                }

                // attributeOffset == 0 means there is no attributes
                if (attributeOffset != 0)
                {
                    VkVertexInputBindingDescription attrBindingDesc;
                    attrBindingDesc.binding = 1;
                    attrBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                    attrBindingDesc.stride = attributeOffset;
                    pipelineCreateInfo.vertexInputBindingDescriptions.push_back(attrBindingDesc);
                }
            }
        }

        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.pNext = VK_NULL_HANDLE;
        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.flags = 0;
        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount =
            pipelineCreateInfo.vertexInputBindingDescriptions.size();
        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions =
            pipelineCreateInfo.vertexInputBindingDescriptions.data();
        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount =
            pipelineCreateInfo.vertexAttributeDescriptions.size();
        pipelineCreateInfo.pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions =
            pipelineCreateInfo.vertexAttributeDescriptions.data();
    }

    return pipelineCreateInfo;
}
} // namespace Gfx
