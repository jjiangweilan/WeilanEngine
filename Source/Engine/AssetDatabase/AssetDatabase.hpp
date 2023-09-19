#pragma once
#include "Core/Asset.hpp"
#include "Internal/AssetData.hpp"
#include <filesystem>
namespace Engine
{
class AssetDatabase
{
public:
    AssetDatabase(const std::filesystem::path& projectRoot);

public:
    // path: relative path as projectRoot/Assets/{path}
    Asset* LoadAsset(std::filesystem::path path);

    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, std::filesystem::path path);

private:
    const std::filesystem::path projectRoot;
    const std::filesystem::path assetPath;
    const std::filesystem::path assetDatabasePath;

    class Assets
    {
    public:
        Asset* Add(const std::filesystem::path& path, std::unique_ptr<AssetData>&& asset);
        AssetData* GetAssetData(const std::filesystem::path& path);

        // get by asset's uuid
        AssetData* GetAssetData(const UUID& uuid);

    private:
        std::unordered_map<std::string, AssetData*> byPath;
        std::unordered_map<UUID, std::unique_ptr<AssetData>> data;
    } assets;

    SerializeReferenceResolveMap referenceResolveMap;
};
} // namespace Engine
