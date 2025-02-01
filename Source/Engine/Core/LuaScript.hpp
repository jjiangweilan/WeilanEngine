#pragma once
#include "Core/Asset.hpp"

class LuaScript : public Asset
{
    DECLARE_ASSET();

    void LoadScript(const std::filesystem::path& path);

public:
    std::filesystem::path scriptAssetPath;
};
