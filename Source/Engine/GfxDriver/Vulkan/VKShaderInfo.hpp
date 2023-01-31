#include "../ShaderInfo.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx::ShaderInfo::Utils
{
VkDescriptorType MapBindingType(BindingType type);
VkShaderStageFlags MapShaderStage(ShaderStageFlags stages);
} // namespace Engine::Gfx::ShaderInfo::Utils
