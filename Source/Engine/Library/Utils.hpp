#pragma once
#include "Engine/Library/DynamicArray.hpp"
#include <string>
namespace Utils
{
std::string strRemoveLastWord(const std::string& str);
std::string strRemoveBlanks(const std::string& str);
std::string strToLower(const std::string& s);
std::vector<std::string> SplitString(const std::string& s, char delimiter);
bool strContians(const std::string& s, const std::string& element);

uint32_t GetPadding(uint32_t address, uint32_t alignment);
} // namespace Utils
