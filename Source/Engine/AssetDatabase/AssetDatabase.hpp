#pragma once
#include "AssetDatabase/Importers/AssetImporter.hpp"
#include "AssetDatabase/Loaders/AssetLoader.hpp"
#include "AssetDatabase/Private/AssetFileSystem.hpp"
#include "AssetDatabase/Private/AsyncLoadProcessor.hpp"
#include "Core/Asset.hpp"
#include "Private/AssetData.hpp"
#include <filesystem>

class Scene;
class AssetDatabase
{
    static AssetDatabase*& SingletonReference();
    std::filesystem::path projectRoot;
    std::filesystem::path assetDirectory;
    std::filesystem::path assetDatabaseDirectory;

    ImportDatabase importDatabase;
    AssetFileSystem assetFileSystem;
    AsyncLoadProcessor asyncLoadProcessor;

    SerializeReferenceResolveMap referenceResolveMap;
    std::vector<std::unique_ptr<AssetData>> assetDatas;
    std::vector<AssetData*> internalAssets;

    bool requestShaderRefresh = false;
    bool requestShaderRefreshAll = false;

public:
    AssetDatabase() {};

    static AssetDatabase* Singleton();
    void Init(const std::filesystem::path& projectRoot);

    const std::filesystem::path& GetAssetDirectory() const;
    const std::vector<AssetData*>& GetInternalAssets() const;
    const std::filesystem::path& GetProjectRoot() const;
    const std::filesystem::path& GetProjectAssetDatabaseDirectory() const;
    const std::vector<std::unique_ptr<AssetData>>& GetAssetData();

    void ReloadScripts();
    void RequestShaderRefresh(bool all = false);
    void RefreshShader();

    std::vector<uint8_t> ReadRawAssetData(const UUID& uuid);
    void SaveDirtyAssets();
    void RemoveAssetData(AssetData* ad);
    // ObjPtr<Asset> LoadAssetAsync(const std::filesystem::path& path);
    Asset* LoadAsset(std::filesystem::path path, bool forceReimport = false);
    Scene* LoadScene(const UUID& sceneUUID);
    ObjPtr<Asset> LoadAssetAsync_Experimental(std::filesystem::path path, bool forceReimport = false);
    Asset* LoadAssetByID(const UUID& uuid, bool forceReimport = false);
    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, std::filesystem::path path);
    bool IsAssetInDatabase(Asset& asset);
    void SaveAsset(Asset& asset);
    void UnloadAsset(Asset& asset);
    nlohmann::json GetAssetMeta(Asset& asset);
    const std::filesystem::path& GetAssetPath(const UUID& uuid);
    void SetAssetMeta(Asset& asset, const nlohmann::json& meta);

    void PollAsyncLoadingResults();

    // file system
    void CreateFolderAtPath(const std::filesystem::path& path);
    void Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
    void Remove(const std::filesystem::path& path);
    std::filesystem::path AbsolutePathToAssetPath(const std::filesystem::path& absolutePath);

    void Reimport(const std::filesystem::path& path);
    void ReimportByID(const UUID& uuid);

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

private:
    AssetData* AddAssetData(std::unique_ptr<AssetData>&& newAssetData);
    void SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path);
    void LoadEngineInternal();
    void ResolveSerializerReference(Serializer& ser, SerializeReferenceResolveMap& resolveMap);
    void LoadAssetDatas();
    void EnsureAllFilesAreImported(const std::filesystem::path& directory);
    void ImportAssetIfNeeded(const std::filesystem::path& path, bool forceReimport);
    bool IsAssetImported(const std::filesystem::path& path);
    const UUID& GetUUIDFromPath(const std::filesystem::path& path);

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
