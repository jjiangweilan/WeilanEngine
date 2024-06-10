#include "../ShaderInfo.hpp"
#include <vulkan/vulkan.h>

namespace Gfx::ShaderInfo::Utils
{
VkDescriptorType MapBindingType(BindingType type);
VkShaderStageFlags MapShaderStage(ShaderStageFlags stages);
} // namespace Gfx::ShaderInfo::Utils
