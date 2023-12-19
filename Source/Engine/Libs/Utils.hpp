#pragma once
#include <cinttypes>
#include <string>
namespace Utils
{
std::string strTolower(const std::string& s);

uint32_t GetPadding(uint32_t address, uint32_t alignment);
} // namespace Utils
