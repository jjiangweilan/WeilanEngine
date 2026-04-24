#pragma once
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Library/EnumFlags.hpp"
#include "ShaderPipelineInfo.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

namespace Gfx
{
// replaced by ShaderPorgramCreateInfo. now CompiledSpv only used in importing process
struct CompiledSpv
{
    std::vector<uint32_t> vertSpv;
    std::vector<uint32_t> vertSpv_noOp;
    std::vector<uint32_t> fragSpv;
    std::vector<uint32_t> fragSpv_noOp;

    std::vector<uint32_t> compSpv;
    std::vector<uint32_t> compSpv_noOp;
};

struct PipelineCreateInfo
{
    std::vector<uint8_t> vertSpv = {};
    std::vector<uint8_t> fragSpv = {};
    std::vector<uint8_t> computeSpv = {};

    ShaderPipelineInfo pipelineInfo = {};
    PipelineConfig defaultConfig = {};
};
} // namespace Gfx
