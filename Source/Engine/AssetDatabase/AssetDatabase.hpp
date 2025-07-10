#pragma once
#include "AssetDatabase/Importers/AssetLoader.hpp"
#include "Core/Asset.hpp"
#include "Core/JobSystem.hpp"
#include "Internal/AssetData.hpp"
#include <filesystem>
#include <set>

class AssetDatabase
{
public:
    AssetDatabase() {};

    void Init(const std::filesystem::path& projectRoot);
    void SaveDirtyAssets();

    // path: relative path as projectRoot/Assets/{path}
    // Asset* LoadAsset(std::filesystem::path path);
    // Asset* LoadAssetByID(const UUID& uuid);

    Asset* LoadAsset(const std::filesystem::path& path, bool forceReimport = false);
    Asset* LoadAssetByID(const UUID& uuid, bool forceReimport = false);
    DynamicArray<uint8_t> ReadRawAssetData(const UUID& uuid);

    DynamicArray<Asset*> LoadAssets(std::span<std::filesystem::path> pathes);

    void UnloadAsset(Asset& asset);
    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, std::filesystem::path path);
    void SaveAsset(Asset& asset);

    bool IsAssetInDatabase(Asset& asset);

    void ReloadScripts();

    void RequestShaderRefresh(bool all = false);
    void RefreshShader();
    const std::filesystem::path& GetAssetPath(const UUID& uuid)
    {
        auto assetData = assets.GetAssetData(uuid);
        if (assetData)
            return assetData->GetAssetPath();

        static std::filesystem::path empty = "";
        return empty;
    }

    // remove assetdata in filesystem
    void RemoveAssetData(AssetData* ad);

    const std::filesystem::path& GetAssetDirectory() const { return assetDirectory; }

    const DynamicArray<AssetData*>& GetInternalAssets() const { return internalAssets; }

    std::filesystem::path AbsolutePathToAssetPath(const std::filesystem::path& absolutePath)
    {
        return std::filesystem::relative(absolutePath, assetDirectory);
    }

    static AssetDatabase* Singleton() { return SingletonReference(); }

    template <std::derived_from<Serializer> S, std::derived_from<Asset> T>
    void CopyThroughSerialization(T& origin, T& copy)
    {
        JsonSerializer s;
        origin.Serialize(&s);

        SerializeReferenceResolveMap resolveMap;
        JsonSerializer de(s.GetBinary(), &resolveMap);
        copy.Deserialize(&de);

        ResolveSerializerReference(de, resolveMap);

        copy.OnLoaded();
    }

    const std::filesystem::path& GetProjectRoot() const { return projectRoot; }

    const std::filesystem::path& GetProjectAssetDatabaseDirectory() const { return assetDatabaseDirectory; }

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
    const DynamicArray<std::unique_ptr<AssetData>>& GetAssetData() { return assets.data; }

    // file system
    void CreateFolderAtPath(const std::filesystem::path& path);
    void Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
    void Remove(const std::filesystem::path& path);

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
            size_t operator()(const std::filesystem::path& path) const { return std::filesystem::hash_value(path); }
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
        DynamicArray<std::unique_ptr<AssetData>> data;
    } assets;

    /**
     * @class ImportProcess
     * @brief a struct used to batch processing loaded assets to enable parallel processing
     *
     */
    struct ImportProcess
    {
        std::shared_ptr<AssetLoader> loader;
        AssetData* assetData = nullptr;
        Asset* asset;

        // Import related data
        bool importNeeded = false;
        JobHandle importJob;
        std::unique_ptr<DynamicArray<std::filesystem::path>> importedAssetFilePaths =
            std::make_unique<DynamicArray<std::filesystem::path>>();

        // Load related data
        bool loadNeeded = false;
        bool isReload = false;
        JobHandle loadJob;
    };

    SerializeReferenceResolveMap referenceResolveMap;
    std::unordered_map<UUID, int*> managedObjectCounters;

    DynamicArray<AssetData*> internalAssets;
    bool requestShaderRefresh = false;
    bool requestShaderRefreshAll = false;

    void SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path);
    void LoadEngineInternal();

    /**
     * @brief Internal implementation to load an asset including all it's reference in an asynced way. Note that
     * reference is resolved by ObjPtr mechanism so it's not directly handled in AssetDatabase, but loading process
     * complete. All the referenced objects should be loaded and OnLoaded on the assets will be called
     *
     * @param path The file path to import.
     * @param forceReimport If true, forces re-importing even if already imported.
     * @param importProcesses List of processes to use during import.
     * @return True if import was successful, false otherwise.
     */
    void LoadAssetInteral(
        std::filesystem::path path,
        bool forceReimport,
        std::vector<ImportProcess>& importProcesses,
        std::set<std::filesystem::path>& loadings
    );
    void LoadAssetByIDInternal(
        const UUID& uuid,
        bool forceReimport,
        std::vector<ImportProcess>& importProcesses,
        std::set<std::filesystem::path>& loadings
    );

    void ResolveSerializerReference(Serializer& ser, SerializeReferenceResolveMap& resolveMap);
    void SyncImportedAssetFiles(AssetData* assetData, const DynamicArray<std::filesystem::path>& newImported);

    // used to set instance
    friend class WeilanEngine;
};

template <class T>
struct LazyLoadedAsset
{
    LazyLoadedAsset(const char* path) : path(path) {}

    T* operator->() { return Get(); }

    T* Get()
    {
        if (loaded == nullptr)
            loaded = (T*)AssetDatabase::Singleton()->LoadAsset(path);

        return loaded;
    }

    T* loaded = nullptr;
    const char* path;
};
