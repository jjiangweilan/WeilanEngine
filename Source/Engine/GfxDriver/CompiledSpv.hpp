#pragma once
#include <vector>

namespace Gfx
{
struct CompiledSpv
{
    std::vector<uint32_t> vertSpv;
    std::vector<uint32_t> vertSpv_noOp; // no optimization
    std::vector<uint32_t> fragSpv;
    std::vector<uint32_t> fragSpv_noOp;

    std::vector<uint32_t> compSpv;
    std::vector<uint32_t> compSpv_noOp;
};
} // namespace Gfx
