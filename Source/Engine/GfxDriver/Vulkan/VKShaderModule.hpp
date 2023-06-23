#pragma once
#include "../DescriptorSetSlot.hpp"
#include "../ShaderModule.hpp"
#include "VKShaderInfo.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
class VKObjectManager;
class VKContext;
} // namespace Engine::Gfx
namespace Engine::Gfx
{
struct ShaderModuleGraphicsPipelineCreateInfos
{
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo;

    // vertex Data
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
    std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
};

class VKShaderModule : public ShaderModule
{
public:
    VKShaderModule(const std::string& name, const unsigned char* code, uint32_t codeByteSize, bool vertInterleaved);
    ~VKShaderModule();

    const ShaderModuleGraphicsPipelineCreateInfos& GetShaderModuleGraphicsPipelineCreateInfos();
    inline const ShaderInfo::ShaderStageInfo& GetShaderInfo() { return shaderInfo; };

private:
    RefPtr<VKContext> context;
    bool vertInterleaved;
    VkShaderModule shaderModule;
    VkShaderStageFlagBits stage;
    uint32_t totalInputSize;
    std::string entryPoint;
    ShaderInfo::ShaderStageInfo shaderInfo;
    void ExtractDataFromJson(const nlohmann::json& jsonInfo);

    const VkPhysicalDeviceProperties& gpuProperties;

    ShaderModuleGraphicsPipelineCreateInfos gpCreateInfos;
    ShaderModuleGraphicsPipelineCreateInfos GenerateShaderModuleGraphicsPipelineCreateInfos();
};

} // namespace Engine::Gfx
