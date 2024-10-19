#pragma once
#include <vector>
#include <nlohmann/json.hpp>

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

struct ShaderProgramCreateInfo
{
    std::vector<uint32_t> vertSpv = {};
    std::vector<uint32_t> fragSpv = {};
    std::vector<uint32_t> compSpv = {};

    nlohmann::json vertReflection;
    nlohmann::json fragReflection;
    nlohmann::json compReflection;
};
} // namespace Gfx
