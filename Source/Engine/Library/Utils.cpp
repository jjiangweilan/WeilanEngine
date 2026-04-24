#include "Utils.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
namespace Utils
{

std::string strRemoveLastWord(const std::string& str)
{
    if (str.empty())
        return str;
    
    // Find the last non-space character
    size_t end = str.find_last_not_of(" \t\n\r");
    if (end == std::string::npos)
        return ""; // String contains only whitespace
    
    // Find the last space before the last word
    size_t lastSpace = str.find_last_of(" \t\n\r", end);
    
    if (lastSpace == std::string::npos)
        return ""; // Only one word, remove it all
    
    return str.substr(0, lastSpace);
}

std::string strToLower(const std::string& ss)
{
    std::string s = ss;
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c)
        { return std::tolower(c); } // correct
    );
    return s;
}

std::string strRemoveBlanks(const std::string& ss)
{
    std::string str = ss;
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    return str;
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

bool strContians(const std::string& s, const std::string& element)
{
    auto iter = s.find(element);
    return iter != s.npos;
}
} // namespace Utils
