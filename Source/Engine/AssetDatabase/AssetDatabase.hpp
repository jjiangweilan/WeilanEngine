#pragma once
#include "AssetDatabase/Importers/AssetLoader.hpp"
#include "Core/Asset.hpp"
#include "Internal/AssetData.hpp"
#include <filesystem>
class AssetDatabase
{
public:
    AssetDatabase() {};

    void Init(const std::filesystem::path& projectRoot);
    void SaveDirtyAssets();

    // path: relative path as projectRoot/Assets/{path}
    // Asset* LoadAsset(std::filesystem::path path);
    // Asset* LoadAssetByID(const UUID& uuid);

    Asset* LoadAsset(std::filesystem::path path, bool forceReimport = false);
    Asset* LoadAssetByID(const UUID& uuid, bool forceReimport = false);

    std::vector<Asset*> LoadAssets(std::span<std::filesystem::path> pathes);

    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, std::filesystem::path path);
    void SaveAsset(Asset& asset);

    bool IsAssetInDatabase(Asset& asset);

    void RequestShaderRefresh(bool all = false);
    void RefreshShader();
    bool ChangeAssetPath(const std::filesystem::path& src, const std::filesystem::path& dst);

    const std::filesystem::path& GetAssetDirectory()
    {
        return assetDirectory;
    }

    const std::filesystem::path& GetAssetDirectory() const
    {
        return assetDirectory;
    }

    const std::vector<AssetData*>& GetInternalAssets() const
    {
        return internalAssets;
    }

    static AssetDatabase* Singleton()
    {
        return SingletonReference();
    }

    template <std::derived_from<Serializer> S, std::derived_from<Asset> T>
    void CopyThroughSerialization(T& origin, T& copy)
    {
        JsonSerializer s;
        origin.Serialize(&s);

        SerializeReferenceResolveMap resolveMap;
        JsonSerializer de(s.GetBinary(), &resolveMap);
        copy.Deserialize(&de);

        ResolveSerializerReference(de, resolveMap);

        copy.OnLoadingFinished();
    }

    const std::filesystem::path& GetProjectRoot() const
    {
        return projectRoot;
    }

    nlohmann::json GetAssetMeta(Asset& asset)
    {
        AssetData* data = assets.GetAssetData(asset.GetUUID());
        if (data)
        {
            return data->GetMeta();
        }

        return nlohmann::json::object();
    }

    void SetAssetMeta(Asset& asset, const nlohmann::json& meta)
    {
        AssetData* data = assets.GetAssetData((asset.GetUUID()));

        if (data)
        {
            data->SetMeta(meta);
        }
    }
    const std::vector<std::unique_ptr<AssetData>>& GetAssetData()
    {
        return assets.data;
    }

private:
    static AssetDatabase*& SingletonReference();
    std::filesystem::path projectRoot;
    std::filesystem::path assetDirectory;
    std::filesystem::path assetDatabaseDirectory;
    ImportDatabase importDatabase;

    class Assets
    {
    public:
        struct PathHasher
        {
            size_t operator()(const std::filesystem::path& path) const
            {
                return std::filesystem::hash_value(path);
            }
        };
        Asset* Add(std::unique_ptr<AssetData>&& asset);

        // used for internal asset, internal asset needs to first Add to Assets but it doesn't have contained objects
        // yet, so after it loads it needs to update
        void UpdateAssetData(AssetData* assetData);
        AssetData* GetAssetData(const std::filesystem::path& path);

        // get by asset's uuid
        AssetData* GetAssetData(const UUID& uuid);

        std::unordered_map<std::filesystem::path, AssetData*, PathHasher> byPath;
        std::unordered_map<UUID, AssetData*> byUUID;
        std::vector<std::unique_ptr<AssetData>> data;
    } assets;

    SerializeReferenceResolveMap referenceResolveMap;
    std::unordered_map<UUID, int*> managedObjectCounters;

    std::vector<AssetData*> internalAssets;
    bool requestShaderRefresh = false;
    bool requestShaderRefreshAll = false;

    void SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path);
    void LoadEngineInternal();

    void ResolveSerializerReference(Serializer& ser, SerializeReferenceResolveMap& resolveMap);

    // used to set instance
    friend class WeilanEngine;
};
