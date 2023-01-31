#include "Utils.hpp"
#include <algorithm>
#include <cctype>
namespace Engine::Utils
{
std::string strTolower(const std::string& ss)
{
    std::string s = ss;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); } // correct
    );
    return s;
}

uint32_t GetPadding(uint32_t address, uint32_t alignment)
{
    if (alignment == 0)
        return 0;

    return (alignment - (address & (alignment - 1))) & (alignment - 1);
}
} // namespace Engine::Utils
