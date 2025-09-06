#include "Buffer.hpp"
#include "ShaderProgram.hpp"

namespace Gfx
{
void* Buffer::CreateUniformBuffer(
    ShaderProgram* shaderProgram, DescriptorSetSemantics descriptorSetSemantics, int nBinding
)
{
    const auto& pipelineInfo = shaderProgram->GetShaderInfo();
    auto descriptorSet = pipelineInfo.GetDescriptorSet(Gfx::DescriptorSetSemantics::Material);
    if (descriptorSet == nullptr)
        return nullptr;

    auto binding = descriptorSet->GetBinding(nBinding);
    if (binding != nullptr && binding->descriptorType == Gfx::DescriptorType::UniformBuffer)
    {}
}
} // namespace Gfx
