#include "Utils.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
namespace Utils
{
std::string strTolower(const std::string& ss)
{
    std::string s = ss;
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) { return std::tolower(c); } // correct
    );
    return s;
}

uint32_t GetPadding(uint32_t address, uint32_t alignment)
{
    if (alignment == 0)
        return 0;

    return (alignment - (address & (alignment - 1))) & (alignment - 1);
}

std::vector<std::string> SplitString(const std::string& s, char delimiter)
{
    std::stringstream ss(s);
    std::vector<std::string> tokens;
    std::string token;
    while (getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}
} // namespace Utils
