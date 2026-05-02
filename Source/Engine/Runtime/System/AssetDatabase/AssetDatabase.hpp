#pragma once
#include "AssetPath.hpp"
#include "Engine/Runtime/System/AssetDatabase/Importers/AssetImporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/Loaders/AssetLoader.hpp"
#include "Engine/Runtime/System/AssetDatabase/Private/AssetFileSystem.hpp"
#include "Engine/Runtime/System/AssetDatabase/Private/AsyncLoadProcessor.hpp"
#include "Engine/Core/Asset.hpp"
#include "Private/AssetData.hpp"
#include <filesystem>

using AbsolutePath = std::filesystem::path;

class Scene;
class AssetDatabase
{
    static AssetDatabase*& SingletonReference();
    AbsolutePath projectRoot;
    AbsolutePath assetDirectory;
    AbsolutePath assetDatabaseDirectory;

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
    void Init(const AbsolutePath& projectRoot);

    const AbsolutePath& GetAssetDirectory() const;
    const std::vector<AssetData*>& GetInternalAssets() const;
    const AbsolutePath& GetProjectRoot() const;
    const AbsolutePath& GetProjectAssetDatabaseDirectory() const;
    const std::vector<std::unique_ptr<AssetData>>& GetAssetData();

    void ReloadScripts();
    void RequestShaderRefresh(bool all = false);
    bool RefreshShader();

    std::vector<uint8_t> ReadRawAssetData(const UUID& uuid);
    void SaveDirtyAssets();
    void RemoveAssetData(AssetData* ad);
    // ObjPtr<Asset> LoadAssetAsync(const std::filesystem::path& path);
    Asset* LoadAsset(const AssetPath& path, bool forceReload = false);
    Scene* LoadScene(const UUID& sceneUUID);
    ObjPtr<Asset> LoadAssetAsync_Experimental(const AssetPath& path, bool forceReload = false);
    Asset* LoadAssetByID(const UUID& uuid, bool forceReload = false);
    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, const AssetPath& path);
    bool IsAssetInDatabase(Asset& asset);
    void SaveAsset(Asset& asset);
    void UnloadAsset(Asset& asset);
    nlohmann::json GetAssetMeta(Asset& asset);
    const AssetPath& GetAssetPath(const UUID& uuid);
    void SetAssetMeta(Asset& asset, const nlohmann::json& meta);

    void SyncLoadingResults();
    void PollAsyncLoadingResults();
    void EnsureAllFilesAreImported();

    // file system
    void CreateFolderAtPath(const AssetPath& path);
    void Rename(const AssetPath& oldPath, const AssetPath& newPath);
    void Remove(const AssetPath& path);

    void Reimport(const AssetPath& path);
    void ReimportByID(const UUID& uuid);
    bool CanImport(const AssetPath& path) const;

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
    void SerializeAssetToDisk(Asset& asset, const AbsolutePath& path);
    void LoadEngineInternal();
    void ResolveSerializerReference(Serializer& ser, SerializeReferenceResolveMap& resolveMap);
    bool IsImportedSubAsset(const Asset& asset) const;
    void LoadAssetDatas();
    void EnsureAllFilesAreImported(const AbsolutePath& directory);
    void ImportAssetIfNeeded(const AssetPath& path, bool forceReimport);
    const UUID& GetUUIDFromPath(const AssetPath& path);

    // used to set instance
    friend class WeilanEngine;
};

template <class T>
struct LazyLoadedAsset
{
    LazyLoadedAsset(const char* path)
        : path(path) {}

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
