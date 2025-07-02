#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include <filesystem>
#include <typeindex>
#include <unordered_map>

class FileIcons
{
public:
    static FileIcons& Instance();
    // enter utf code picked from here: https://www.nerdfonts.com/cheat-sheet
    static std::string Utf16ToUtf8(char16_t utf16_codepoint);
    static std::string GetIcon(const std::filesystem::path& ext);

    LazyLoadedAsset<Texture> fileIcon = "_engine_internal/Editor/Icons/folder.png";

private:
    static std::unordered_map<std::string, std::string> toIcon;
    static std::unordered_map<std::type_index, char16_t> toIconType;
};
