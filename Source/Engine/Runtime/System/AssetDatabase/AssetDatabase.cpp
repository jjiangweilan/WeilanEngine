#include "AssetDatabase.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/System/AssetDatabase/Importers/AssetImporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/Loaders/AssetLoader.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBackend.hpp"
#include "Engine/Runtime/System/EngineConfig.hpp"
#include <future>
#include <iostream>
#include <spdlog/spdlog.h>

AssetDatabase* AssetDatabase::Singleton()
{
    return SingletonReference();
}

nlohmann::json AssetDatabase::GetAssetMeta(Asset& asset)
{
    AssetData* data = assetFileSystem.GetAssetData(asset.GetUUID());
    if (data)
    {
        return data->GetMeta();
    }
    return nlohmann::json::object();
}

void AssetDatabase::SetAssetMeta(Asset& asset, const nlohmann::json& meta)
{
    AssetData* data = assetFileSystem.GetAssetData(asset.GetUUID());
    if (data)
    {
        data->SetMeta(meta);
    }
}
const std::vector<std::unique_ptr<AssetData>>& AssetDatabase::GetAssetData()
{
    return assetDatas;
}

void AssetDatabase::Init(const AbsolutePath& projectRoot)
{
    EngineConfig::SetProjectRoot(projectRoot);
    this->projectRoot = projectRoot;
    this->assetDirectory = projectRoot / "Assets";
    this->assetDatabaseDirectory = projectRoot / "AssetDatabase";

    assetFileSystem.Init(projectRoot);
    importDatabase.Init(projectRoot / "ImportDatabase");
    asyncLoadProcessor.Init(&importDatabase, &assetFileSystem, assetDirectory, projectRoot);

    if (!std::filesystem::exists(assetDirectory))
    {
        std::filesystem::create_directory(assetDirectory);
    }

    if (!std::filesystem::exists(assetDatabaseDirectory))
    {
        std::filesystem::create_directory(assetDatabaseDirectory);
    }

    if (!std::filesystem::exists(projectRoot / "ImportDatabase"))
    {
        std::filesystem::create_directory(projectRoot / "ImportDatabase");
    }

    LoadAssetDatas();
    LoadEngineInternal();
    EnsureAllFilesAreImported(assetDirectory);
}

void AssetDatabase::SaveAsset(Asset& asset)
{
    if (asset.IsExternalAsset() || HasFlag(asset.GetFlags(), AssetState::DontSave))
        return;
    // this method only saves asset that is already imported
    AssetData* assetData = assetFileSystem.GetAssetData(asset.GetUUID());

    if (assetData != nullptr)
    {
        SerializeAssetToDisk(asset, assetData->GetAssetAbsolutePath());
    }
}

bool AssetDatabase::IsAssetInDatabase(Asset& asset)
{
    return assetFileSystem.GetAssetData(asset.GetUUID()) != nullptr;
}

void AssetDatabase::Reimport(const AssetPath& path)
{
    ImportAssetIfNeeded(path, true);
}

void AssetDatabase::ReimportByID(const UUID& uuid)
{
    auto assetPath = assetFileSystem.GetAssetData(uuid)->GetAssetPath();
    ImportAssetIfNeeded(assetPath, true);
}

Asset* AssetDatabase::LoadAssetByID(const UUID& uuid, bool forceReload)
{
    auto assetData = assetFileSystem.GetAssetData(uuid);
    if (assetData)
    {
        auto asset = LoadAsset(assetData->GetAssetPath(), forceReload);

        if (asset)
        {
            if (asset->GetUUID() == uuid)
            {
                return asset;
            }
            else
            {
                auto internalAssets = asset->GetInternalAssets();
                auto iter = std::find_if(
                    internalAssets.begin(),
                    internalAssets.end(),
                    [&uuid](Asset* a)
                    { return a->GetUUID() == uuid; }
                );
                if (iter != internalAssets.end())
                    return *iter;
            }
        }
    }

    return nullptr;
}

void AssetDatabase::SerializeAssetToDisk(Asset& asset, const AbsolutePath& path)
{
    JsonSerializer ser;
    asset.Serialize(&ser);
    auto binary = ser.GetBinary();
    if (binary.size() != 0)
    {
        std::ofstream out;
        out.open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
        if (out.is_open() && out.good())
        {
            out.write((char*)binary.data(), binary.size());
        }
    }
}
Asset* AssetDatabase::SaveAsset(std::unique_ptr<Asset>&& a, const AssetPath& path)
{
    if (HasFlag(a->GetFlags(), AssetState::DontSave))
    {
        return a.get();
    }

    auto fullPath = (std::filesystem::path)path;
    fullPath.replace_extension(a->GetExtension());
    int index = 1;
    const std::string stem = path.GetFileNameWithoutExtension();
    const std::string extension = a->GetExtension();
    const std::filesystem::path parentPath = fullPath.parent_path();

    while (std::filesystem::exists(fullPath))
    {
        auto newFilename = fmt::format("{} {}{}", stem, index, extension);
        fullPath = parentPath / newFilename;
        index++;
    }

    AssetPath finalAssetPath = AssetPath(fullPath);
    // only internal asset can be created
    if (!a->IsExternalAsset())
    {
        if (!std::filesystem::exists(fullPath))
        {
            std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), finalAssetPath, projectRoot);
            newAssetData->SaveToDisk(projectRoot);
            Asset* asset = newAssetData->GetAsset();

            SerializeAssetToDisk(*asset, newAssetData->GetAssetAbsolutePath());

            return asset;
        }
        else
        {
            if (AssetData* ad = assetFileSystem.GetAssetData(finalAssetPath))
            {
                auto asset = ad->SetAsset(std::move(a), projectRoot);
                SerializeAssetToDisk(*asset, ad->GetAssetAbsolutePath());
            }
        }
    }
    return nullptr;
}

void AssetDatabase::SaveDirtyAssets()
{
    for (auto& a : assetDatas)
    {
        Asset* asset = a->GetAsset();
        if (asset)
        {
            if (asset->IsDirty())
            {
                SaveAsset(*asset);
                asset->SetDirty(false);
            }
        }

        if (a->IsDirty())
        {
            a->SaveToDisk(projectRoot);
        }
    }
}

void AssetDatabase::LoadEngineInternal()
{
    ShaderLibrary::CompileAllDefaultShaders();

    std::vector<AssetPath> pathes;
    auto cwdAssetsDir = std::filesystem::current_path() / "Assets";
    for (auto entry : std::filesystem::recursive_directory_iterator(cwdAssetsDir))
    {
        if (!entry.is_directory())
        {
            AssetPath ap(entry.path());
            if (!ap.empty() && AssetRegistry::IsExtensionAnAsset(ap.GetExtension()))
            {
                pathes.push_back(ap);
            }
        }
    }

    std::vector<AssetPath> importPathes;
    std::vector<AssetData*> validAssetData;
    for (int i = 0; i < pathes.size(); ++i)
    {
        UUID assetDataUUID(pathes[i].string(), UUID::FromStrTag{});
        auto assetData = assetFileSystem.GetAssetData(assetDataUUID);
        if (assetData == nullptr)
        {
            auto newAssetData =
                std::make_unique<AssetData>(assetDataUUID, pathes[i], AssetData::InternalAssetDataTag{});
            assetData = newAssetData.get();

            AddAssetData(std::move(newAssetData));
        }

        if (assetData->IsValid())
        {
            internalAssets.push_back(assetData);
            validAssetData.push_back(assetData);
            importPathes.push_back(assetData->GetAssetPath());
        }
    }

    for (auto& p : importPathes)
    {
        ImportAssetIfNeeded(p, false);
    }

    for (auto& p : importPathes)
    {
        LoadAsset(p);
    }
    // LoadAssets(others);

    for (auto a : validAssetData)
    {
        a->SaveToDisk(projectRoot);
    }
}

void AssetDatabase::RequestShaderRefresh(bool all)
{
    requestShaderRefresh = true;
    requestShaderRefreshAll = all;
}

static std::future<bool> shaderCompileFuture;
static bool isCompilingShaders = false;

bool AssetDatabase::RefreshShader()
{
    if (requestShaderRefresh)
    {
        requestShaderRefresh = false;
        
        // Launch compilation asynchronously if not already running
        if (!isCompilingShaders)
        {
            isCompilingShaders = true;
            shaderCompileFuture = std::async(std::launch::async, []() {
                return ShaderLibrary::TriggerShaderRecompilation();
            });
        }
    }

    // Check if the background compilation task has finished
    if (isCompilingShaders && shaderCompileFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        GetGfxDriver()->WaitForIdle();

        isCompilingShaders = false;
        bool success = shaderCompileFuture.get();
        requestShaderRefreshAll = false;
        
        if (success)
        {
            // Now that compilation is done, safely reload shaders and materials on the main thread
            ShaderLibrary::ReloadAllShaders(false);
            Material::RebuildAllMaterials();
            return true;
        }
    }
    
    return false;
}

AssetDatabase*& AssetDatabase::SingletonReference()
{
    static AssetDatabase* instance;
    return instance;
}

void AssetDatabase::ResolveSerializerReference(Serializer& ser, SerializeReferenceResolveMap& resolveMap)
{
    auto ResolveAll = [this](std::vector<SerializeReferenceResolve>& resolves, Object* resolved)
    {
        while (!resolves.empty())
        {
            auto& toresolve = resolves.back();
            if (toresolve.target != nullptr)
                *toresolve.target = resolved;
            if (toresolve.callback)
                toresolve.callback(resolved);

            resolves.pop_back();
        }
    };
    for (auto& iter : resolveMap)
    {
        if (iter.second.empty())
            continue;
        // resolve internal reference
        const auto& objs = ser.GetContainedObjects();
        auto containedObj = objs.find(iter.first);
        if (containedObj != objs.end())
        {
            auto resolved = containedObj->second;
            ResolveAll(iter.second, resolved);
        }

        // resolve external reference
        Asset* externalAsset = LoadAssetByID(iter.first);
        if (externalAsset)
        {
            ResolveAll(iter.second, externalAsset);
        }

        // add whatever is not resolved to assetDatabase's resolve map
        if (!iter.second.empty())
        {
            auto& vec = referenceResolveMap[iter.first];
            for (auto& r : iter.second)
            {
                vec.emplace_back(r.target, r.targetUUID, r.callback);
            }
        }
    }
}

ObjPtr<Asset> AssetDatabase::LoadAssetAsync_Experimental(const AssetPath& path, bool forceReload)
{
    auto loaded = asyncLoadProcessor.AsyncLoadFromPath(path);

    return loaded;
}

Asset* AssetDatabase::LoadAsset(const AssetPath& path, bool forceReload)
{
    asyncLoadProcessor.SyncLoad(); // this is used to avoid loading an asset in main thread while it's also loading in async load processor

    ImportAssetIfNeeded(path, false);
    // SCOPED_PROFILER(fmt::format("load asset {}", path.string()));

    /* Debug Comment */
    // std::filesystem::path debugPath = "SceneLit.shad";
    // if (Utils::strContians(path.string(), debugPath.string()))
    // {
    //     spdlog::info("{}", debugPath.string());
    // }

    // copy json meta is slow, so we use pointer here
    static nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json* assetMeta = &empty;

    // find the asset if it's already imported
    auto assetData = assetFileSystem.GetAssetData(path);

    if (assetData == nullptr)
        return nullptr;

    auto absoluteAssetPath = assetData->GetAssetAbsolutePath();

    if (!std::filesystem::exists(absoluteAssetPath))
        return nullptr;

    // this asset is already imported once, we can read its meta
    ASSERT(assetData != nullptr);
    assetMeta = &assetData->GetMeta();

    // override the asset path because this asset may be an internal asset
    absoluteAssetPath = assetData->GetAssetAbsolutePath();

    std::filesystem::path ext = absoluteAssetPath.extension();
    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());
    if (loader == nullptr)
        return nullptr;

    loader->Setup(&importDatabase, absoluteAssetPath, *assetMeta);

    Asset* asset = assetData ? assetData->GetAsset() : nullptr;
    bool loadNeeded = asset == nullptr || forceReload;
    bool isReload = asset == nullptr;

    // no import and load process taken, this asset is ready to be used
    if (!loadNeeded)
    {
        return asset;
    }

    loader->Load();
    std::unique_ptr<Asset> newAsset = loader->RetrieveAsset();

    // failed to load asset
    if (newAsset == nullptr)
    {
        return nullptr;
    }

    asset = newAsset.get();

    // this needs to be done after importing becuase if not we don't have internal game object's name to
    // set UUID by SetAsset(implementation detail leakage, refactor may be needed). It also needs to
    // happen before reference resolve so that it has the correct UUID
    asset = assetData->SetAsset(std::move(newAsset), projectRoot);

    // this asset has a aseet data and is already loaded, it's a reload!
    if (forceReload)
    {
        loader->HandleReload(asset);
    }

    // newly imported or loaded, resolve references
    Serializer* serializer;
    SerializeReferenceResolveMap* localResolveMap;
    loader->GetReferenceResolveData(serializer, localResolveMap);

    if (serializer)
    {
        for (auto& uuid : serializer->GetReferencedObjects())
        {
            LoadAssetByID(uuid);
        }
    }

    asyncLoadProcessor.SyncLoad(); // sync internally triggered async loading by this LoadAsset calls
    asset->OnLoaded();
    return asset;
}

void AssetDatabase::CreateFolderAtPath(const AssetPath& path)
{
    int i = -1;
    std::string fileName;
    do
    {
        i++;
        fileName = fmt::format("{}/{} {}", (assetDirectory / path.ToFilesystemPath()).string(), "New Folder", i);
    }
    while (std::filesystem::exists(fileName));
    std::filesystem::create_directory(fileName);
}

void AssetDatabase::Rename(const AssetPath& oldPath, const AssetPath& newPath)
{
    // TODO: sync async works before accessing assetFileSystem
    assetFileSystem.Rename(oldPath, newPath);
}

void AssetDatabase::Remove(const AssetPath& path)
{
    auto absolutePath = assetDirectory / path.ToFilesystemPath();
    if (std::filesystem::is_directory(absolutePath))
    {
        for (auto iter : std::filesystem::directory_iterator(absolutePath))
        {
            Remove(AssetPath(iter.path()));
        }
        std::filesystem::remove(absolutePath);
    }
    else
    {
        // TODO: sync async works before accessing assetFileSystem
        AssetData* assetData = assetFileSystem.GetAssetData(path);

        if (assetData)
        {
            assetDatas.erase(
                std::remove_if(assetDatas.begin(), assetDatas.end(), [&](auto& d)
                               { return d.get() == assetData; })
            );
        }

        assetFileSystem.Remove(path);
    }
}

void AssetDatabase::RemoveAssetData(AssetData* assetData)
{
    std::error_code e;
    std::filesystem::remove(GetProjectAssetDatabaseDirectory() / assetData->GetAssetDataUUID().ToString(), e);

    if (e.value() == 0)
    {
        auto iter = std::find_if(
            assetDatas.begin(),
            assetDatas.end(),
            [assetData](const std::unique_ptr<AssetData>& dd)
            { return dd.get() == assetData; }
        );

        if (iter != assetDatas.end())
        {
            assetDatas.erase(iter);
        }

        assetFileSystem.RemoveAssetData(assetData);
    }
}

void AssetDatabase::UnloadAsset(Asset& asset)
{
    // TODO: sync async works before accessing assetFileSystem
    assetFileSystem.UnloadAsset(asset);
}

Scene* AssetDatabase::LoadScene(const UUID& sceneUUID)
{
    auto assetData = assetFileSystem.GetAssetData(sceneUUID);
    if (assetData)
    {
        auto scene = asyncLoadProcessor.LoadAssetJob(assetData->GetAssetPath(), assetData);
        asyncLoadProcessor.SyncLoad(); // make sure all dependent assets are loaded
        if (scene)
        {
            Asset* scenePtr = assetData->SetAsset(std::move(scene), projectRoot);
            scenePtr->OnLoaded();
            return static_cast<Scene*>(scenePtr);
        }
    }

    return nullptr;
}

void AssetDatabase::ReloadScripts()
{
    auto gameScripts = Object::GetObjectsOfType<GameScript>();
    std::vector<JsonSerializer> serializers(gameScripts.size());
    for (size_t i = 0; i < gameScripts.size(); ++i)
    {
        gameScripts[i]->LuaSerialize(&serializers[i]);
    }

    auto luaBackend = LuaBackend::GetInstance();
    luaBackend->Destroy();
    luaBackend->Init(GetAssetDirectory().string().c_str());

    auto luaScripts = Object::GetObjectsOfType<LuaScript>();
    for (auto& lg : luaScripts)
    {
        lg->ReloadScript();
    }

    for (size_t i = 0; i < gameScripts.size(); ++i)
    {
        auto g = gameScripts[i];
        g->ReloadScript();
        g->LuaDeserialize(&serializers[i]);
    }
}

std::vector<uint8_t> AssetDatabase::ReadRawAssetData(const UUID& uuid)
{
    auto assetData = assetFileSystem.GetAssetData(uuid);
    if (assetData)
    {
        auto absolutePath = assetData->GetAssetAbsolutePath();
        if (std::filesystem::exists(absolutePath))
        {
            std::ifstream f(absolutePath, std::ios::binary);
            size_t fileSize = std::filesystem::file_size(absolutePath);
            std::vector<uint8_t> binary(fileSize);
            f.read((char*)binary.data(), fileSize);

            return binary;
        }
    }

    return {};
}

void AssetDatabase::LoadAssetDatas()
{
    // we need to load all the already imported asset when AssetDatabase starts so that when user load an asset we
    // know it's already in the database
    for (auto const& dirEntry : std::filesystem::directory_iterator{assetDatabaseDirectory})
    {
        if (dirEntry.is_regular_file())
        {
            // assetData's file name is it's UUID
            UUID uuid(dirEntry.path().filename().string());
            auto ad = std::make_unique<AssetData>(uuid, projectRoot);

            if (ad->IsValid())
            {
                AddAssetData(std::move(ad));
            }
            else
            {
                // TODO: is AssetData is not valid... do something with it!
            }
        }
    }
}

// ObjPtr<Asset> AssetDatabase::LoadAssetAsync(const AbsolutePath& path)
//{
//     ObjPtr<Asset> ptr = asyncLoadProcessor.AsyncLoadFromPath(path);
//
//     return ptr;
// }

const UUID& AssetDatabase::GetUUIDFromPath(const AssetPath& path)
{
    if (AssetData* assetData = assetFileSystem.GetAssetData(path))
    {
        return assetData->GetAssetUUID();
    }

    return UUID::GetEmptyUUID();
}

const AssetPath& AssetDatabase::GetAssetPath(const UUID& uuid)
{
    auto assetData = assetFileSystem.GetAssetData(uuid);
    if (assetData)
        return assetData->GetAssetPath();

    static AssetPath empty = "";
    return empty;
}

const AbsolutePath& AssetDatabase::GetAssetDirectory() const
{
    return assetDirectory;
}

const std::vector<AssetData*>& AssetDatabase::GetInternalAssets() const
{
    return internalAssets;
}

const AbsolutePath& AssetDatabase::GetProjectRoot() const
{
    return projectRoot;
}

const AbsolutePath& AssetDatabase::GetProjectAssetDatabaseDirectory() const
{
    return assetDatabaseDirectory;
}

AssetData* AssetDatabase::AddAssetData(std::unique_ptr<AssetData>&& newAssetData)
{
    if (newAssetData != nullptr)
    {
        auto ptr = newAssetData.get();
        assetFileSystem.Add(ptr);
        assetDatas.push_back(std::move(newAssetData));
        return ptr;
    }

    return nullptr;
}

void AssetDatabase::EnsureAllFilesAreImported(const AbsolutePath& directory)
{
    for (auto const& dirEntry : std::filesystem::directory_iterator{directory})
    {
        if (dirEntry.is_regular_file())
        {
            const auto& path = dirEntry.path();

            auto assetPath = AssetPath(path);
            ImportAssetIfNeeded(assetPath, false);
        }
        else if (dirEntry.is_directory())
        {
            EnsureAllFilesAreImported(dirEntry.path());
        }
    }
}

void AssetDatabase::ImportAssetIfNeeded(const AssetPath& path, bool forceReimport)
{
    std::filesystem::path stdPath = path.ToFilesystemPath();
    auto ext = stdPath.extension();
    std::unique_ptr<AssetImporter> importer = AssetImporterRegistry::CreateAssetImporterByExtension(ext.string());

    if (importer == nullptr)
        return;

    static nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json* assetMeta = &empty;

    auto assetData = assetFileSystem.GetAssetData(path);

    // this asset is already imported once, we can read its meta
    if (!assetData)
    {
        assetData = AddAssetData(std::make_unique<AssetData>(path, projectRoot));
    }

    // override the asset path because this asset may be an internal asset
    assetMeta = &assetData->GetMeta();
    auto absoluteAssetPath = assetData->GetAssetAbsolutePath();

    if (!std::filesystem::exists(absoluteAssetPath))
        return;

    importer->Setup(importDatabase, absoluteAssetPath, *assetMeta);

    bool importNeeded = forceReimport || importer->ImportNeeded();
    std::vector<AssetPath> importedAssetFilePaths;
    if (importNeeded)
    {
        auto stdImported = importer->Import();
        for (auto& p : stdImported)
            importedAssetFilePaths.push_back(AssetPath(p));

        if (assetData != nullptr)
        {
            assetFileSystem.SyncImportedAssetFiles(assetData, importedAssetFilePaths);
        }
    }

    assetData->SetMeta(importer->GetMeta());
    assetData->SaveToDisk(projectRoot);
}

void AssetDatabase::PollAsyncLoadingResults()
{
    asyncLoadProcessor.PollAsyncLoading();
}

void AssetDatabase::SyncLoadingResults()
{
    asyncLoadProcessor.SyncLoad();
}
