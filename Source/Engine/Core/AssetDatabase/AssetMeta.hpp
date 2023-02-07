#pragma once
#include "Libs/UUID.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string_view>

namespace Engine
{

// this class represent the .asset file that associate with the original asset file
class AssetMeta
{
public:
    AssetMeta(const std::filesystem::path& assetPath);
    ~AssetMeta();

    UUID ReadUUID();
    void WriteUUID(const UUID& uuid);

    void AddSubasset(UUID uuid);

    std::filesystem::path GetMainAssetPath();
    std::span<UUID> GetSubassets();

    template <class T>
    void Write(const std::string& key, const T& val)
    {
        changed = true;
        file[key] = val;
    }

    template <class T>
    T Read(std::string_view key, const T& defaultVal)
    {
        return file.value(key, defaultVal);
    }

private:
    std::filesystem::path path;
    nlohmann::json file;
    bool changed = false;
};
} // namespace Engine
