#pragma once
#include "AssetMeta.hpp"
#include <fstream>
namespace Engine
{
AssetMeta::AssetMeta(const std::filesystem::path& assetPath) : path(assetPath)
{
    if (!std::filesystem::exists(assetPath))
    {
        return;
    }
    std::ifstream f(assetPath);
    if (f.good())
    {
        file = nlohmann::json::parse(f);
    }
}

AssetMeta::~AssetMeta()
{
    if (changed)
    {
        std::ofstream f(path, std::ios::trunc);
        if (f.good())
        {
            f << file.dump();
        }
    }
}
} // namespace Engine
