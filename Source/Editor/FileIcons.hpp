#pragma once
#include <filesystem>
#include <unordered_map>
#include <typeindex>

class FileIcons
{
public:
    // enter utf code picked from here: https://www.nerdfonts.com/cheat-sheet
    static std::string Utf16ToUtf8(char16_t utf16_codepoint);
    static std::string GetIcon(const std::filesystem::path& ext);

private:
    static std::unordered_map<std::string, std::string> toIcon;
    static std::unordered_map<std::type_index, char16_t> toIconType;
};
