#include "Shader.hpp"
DEFINE_OBJECT(Object, Shader, "B1E88B63-32EF-4690-B694-EE8F23BF7E72");

int Shader::GetSet(const std::string& name)
{
    auto set = GetShaderProgram()->GetShaderInfo().GetDescriptorSet(name);
    if (set)
    {
        return set->setNum;
    }
    return 0;
}

int Shader::GetSet(Gfx::DescriptorSetSemantics setSlot)
{
    auto set = GetShaderProgram()->GetShaderInfo().GetDescriptorSet(setSlot);
    if (set)
    {
        return set->setNum;
    }
    return -1;
}
