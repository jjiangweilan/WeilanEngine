#pragma once
#include "GfxDriver/ShaderConfig.hpp"

#include <string>
namespace Engine::Utils
{
    Gfx::BlendFactor MapBlendFactor(const std::string& str);
    Gfx::CullMode MapCullMode(const std::string str);
    Gfx::BlendOp MapBlendOp(const std::string& str);
    Gfx::CompareOp MapCompareOp(const std::string& str);
    Gfx::StencilOp MapStencilOp(const std::string& str);

    const char* MapStrBlendFactor(Gfx::BlendFactor factor);
    const char* MapStrCullMode(Gfx::CullMode cull);
    const char* MapStrBlendOp(Gfx::BlendOp op);
    const char* MapStrCompareOp(Gfx::CompareOp op);
    const char* MapStrStencilOp(Gfx::StencilOp op);
}
