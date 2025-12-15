#include "ShaderPipelineInfo.hpp"

Gfx::ShaderDynamicState Gfx::StringToShaderDynamicState(const std::string& str)
{
    if (str == "depthBiasEnable")
        return ShaderDynamicState::DepthBiasEnable;
    else if (str == "depthBias")
        return ShaderDynamicState::DepthBias;

    return ShaderDynamicState::None;
}
