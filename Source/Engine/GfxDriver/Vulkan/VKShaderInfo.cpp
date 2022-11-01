#include "VKShaderInfo.hpp"
namespace Engine::Gfx::ShaderInfo::Utils
{
    VkDescriptorType MapBindingType(BindingType type)
    {
        switch (type)
        {
            case BindingType::SSBO: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case BindingType::Texture: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case BindingType::UBO: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case BindingType::SeparateImage: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case BindingType::SeparateSampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            assert(0 && "Map BindingType failed");
        }

        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }

    VkShaderStageFlags MapShaderStage(ShaderStageFlags stages)
    {
        using namespace ShaderStage;
        VkShaderStageFlags rtn = 0;
        if (stages & Vert) rtn |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stages & Frag) rtn |= VK_SHADER_STAGE_FRAGMENT_BIT;

        return rtn;
    }
}
