#include "AssetDatabase.hpp"
#include "AssetDatabase/Importers/AssetLoader.hpp"
#include "Core/Component/GameScript.hpp"
#include "Scripting/LuaBackend.hpp"
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

void AssetDatabase::Init(const std::filesystem::path& projectRoot)
{
    this->projectRoot = projectRoot;
    this->assetDirectory = projectRoot / "Assets";
    this->assetDatabaseDirectory = projectRoot / "AssetDatabase";
    this->importDatabase.Init(projectRoot / "ImportDatabase");

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

std::vector<Asset*> AssetDatabase::LoadAssets(std::span<std::filesystem::path> pathes)
{
    struct AsyncImport
    {
        // 0: init, 1: imported, 2: invalid, 3. async external import 4. async internal import
        int stateTrack;
        std::filesystem::path absoluteAssetPath;
        AssetData* assetData = nullptr;
        std::unique_ptr<Asset> newAsset = nullptr;
        std::future<void> import;
        std::unique_ptr<SerializeReferenceResolveMap> resolveMap = nullptr;
        std::unique_ptr<JsonSerializer> ser = nullptr;
    };

    std::vector<std::filesystem::path> validPathes{};
    for (auto& path : pathes)
    {
        if (AssetRegistry::IsExtensionAnAsset(path.extension().string()))
        {
            validPathes.push_back(path);
        }
    }

    const int size = validPathes.size();
    std::vector<Asset*> results(size, nullptr);
    std::vector<AsyncImport> asyncImport(size);

    for (int i = 0; i < validPathes.size(); ++i)
    {
        auto& path = validPathes[i];
        auto assetData = assetFileSystem.GetAssetData(path);
        asyncImport[i].assetData = assetData;
        asyncImport[i].absoluteAssetPath = assetDirectory / path;
        if (assetData)
        {
            // override the asset path because this asset may be an internal asset
            asyncImport[i].absoluteAssetPath = assetData->GetAssetAbsolutePath();
            auto a = assetData->GetAsset();
            if (a)
            {
                results[i] = a;
                asyncImport[i].stateTrack = 1;
            }
        }

        if (!std::filesystem::exists(asyncImport[i].absoluteAssetPath))
        {
            asyncImport[i].stateTrack = 2;
        }
    }

    for (int i = 0; i < size; ++i)
    {
        if (asyncImport[i].stateTrack == 0)
        {
            std::filesystem::path ext = asyncImport[i].absoluteAssetPath.extension();
            asyncImport[i].newAsset = AssetRegistry::CreateAssetByExtension(ext.string());

            if (asyncImport[i].newAsset->IsExternalAsset())
            {
                asyncImport[i].stateTrack = 3;
                asyncImport[i].import = std::async(
                    std::launch::async,
                    [asyncImport = &asyncImport[i]]()
                    { asyncImport->newAsset->LoadFromFile(asyncImport->absoluteAssetPath.string().c_str()); }
                );
            }
            else
            {
                std::ifstream f(asyncImport[i].absoluteAssetPath, std::ios::binary);
                if (f.is_open() && f.good())
                {
                    asyncImport[i].stateTrack = 4;
                    size_t fileSize = std::filesystem::file_size(asyncImport[i].absoluteAssetPath);
                    std::vector<uint8_t> binary(fileSize);
                    f.read((char*)binary.data(), fileSize);
                    asyncImport[i].resolveMap = std::make_unique<SerializeReferenceResolveMap>();
                    asyncImport[i].ser = std::make_unique<JsonSerializer>(binary, asyncImport[i].resolveMap.get());
                    asyncImport[i].import = std::async(
                        std::launch::async,
                        [asyncImport = &asyncImport[i]]()
                        { asyncImport->newAsset->Deserialize(asyncImport->ser.get()); }
                    );
                }
                else
                {
                    asyncImport[i].stateTrack = 1;
                }
            }
        }
    }

    for (int i = 0; i < size; ++i)
    {
        if (asyncImport[i].stateTrack == 3 || asyncImport[i].stateTrack == 4)
        {
            asyncImport[i].import.wait();
            results[i] = asyncImport[i].newAsset.get();
            if (asyncImport[i].assetData)
            {
                asyncImport[i].assetData->SetAsset(std::move(asyncImport[i].newAsset), projectRoot);
            }
            // a new asset needs to be recored/imported in assetDatabase
            else
            {
                std::unique_ptr<AssetData> ad =
                    std::make_unique<AssetData>(std::move(asyncImport[i].newAsset), validPathes[i], projectRoot);
                ad->SaveToDisk(projectRoot);

                AddAssetData(std::move(ad));
            }
        }

        if (asyncImport[i].stateTrack == 4)
        {
            auto& referencedObjects = asyncImport[i].ser->GetReferencedObjects();
            const auto& managedObjectCounters = asyncImport[i].ser->GetManagedObjects();
            this->managedObjectCounters.insert(managedObjectCounters.begin(), managedObjectCounters.end());

            for (auto& uuid : referencedObjects)
            {
                LoadAssetByID(uuid);
            }

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

            for (auto& iter : *asyncImport[i].resolveMap)
            {
                if (iter.second.empty())
                    continue;
                // resolve external reference
                Asset* externalAsset = LoadAssetByID(iter.first);
                if (externalAsset)
                {
                    ResolveAll(iter.second, externalAsset);
                }

                // resolve internal reference
                // currently I didn't resolve reference to external contained object
                // that can be done by cache a list of contained objects
                const auto& objs = (*asyncImport[i].ser).GetContainedObjects();
                auto containedObj = objs.find(iter.first);
                if (containedObj != objs.end())
                {
                    auto resolved = containedObj->second;
                    ResolveAll(iter.second, resolved);
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

        // see if there is any reference need to be resolved to this object
        if (results[i] != nullptr)
        {
            auto iter = referenceResolveMap.find(results[i]->GetUUID());
            if (iter != referenceResolveMap.end())
            {
                for (auto& resolve : iter->second)
                {
                    if (resolve.target != nullptr)
                        *resolve.target = results[i];
                    if (resolve.callback)
                    {
                        resolve.callback(results[i]);
                    }
                }
                referenceResolveMap.erase(iter);
            }
        }
    }

    for (auto a : results)
    {
        if (a != nullptr)
            a->OnLoaded();
    }
    return results;
}

Asset* AssetDatabase::LoadAssetByID(const UUID& uuid, bool forceReimport)
{
    auto assetData = assetFileSystem.GetAssetData(uuid);
    if (assetData)
    {
        auto asset = LoadAsset(assetData->GetAssetPath(), forceReimport);

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
                    [&uuid](Asset* a) { return a->GetUUID() == uuid; }
                );
                if (iter != internalAssets.end())
                    return *iter;
            }
        }
    }

    return nullptr;
}

void AssetDatabase::SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path)
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
Asset* AssetDatabase::SaveAsset(std::unique_ptr<Asset>&& a, std::filesystem::path path)
{
    if (path.is_absolute())
        return nullptr;

    if (HasFlag(a->GetFlags(), AssetState::DontSave))
    {
        return a.get();
    }

    path.replace_extension(a->GetExtension());
    auto fullPath = assetDirectory / path;
    int index = 1;
    const std::filesystem::path& stem = path.stem();
    const std::filesystem::path& extension = path.extension();
    const std::filesystem::path& parentPath = fullPath.parent_path();

    while (std::filesystem::exists(fullPath))
    {
        auto newFilename = fmt::format("{} {}{}", stem.string(), index, extension.string());
        fullPath = parentPath / newFilename;
        index++;
    }

    path = std::filesystem::relative(fullPath, GetAssetDirectory());
    // only internal asset can be created
    if (!a->IsExternalAsset())
    {
        if (!std::filesystem::exists(fullPath))
        {
            std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), path, projectRoot);
            newAssetData->SaveToDisk(projectRoot);
            Asset* asset = newAssetData->GetAsset();

            SerializeAssetToDisk(*asset, newAssetData->GetAssetAbsolutePath());

            return asset;
        }
        else
        {
            if (AssetData* ad = assetFileSystem.GetAssetData(path))
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
    std::vector<std::string> pathes;
    for (auto entry : std::filesystem::recursive_directory_iterator("./Assets"))
    {
        if (!entry.is_directory())
        {
            auto relative = std::filesystem::relative(entry.path(), "./Assets/");
            if (AssetRegistry::IsExtensionAnAsset(relative.extension().string()))
            {
                auto str = relative.string();
                std::replace(str.begin(), str.end(), '\\', '/');
                pathes.push_back(str);
            }
        }
    }

    std::vector<std::filesystem::path> importPathes;
    std::vector<AssetData*> validAssetData;
    for (int i = 0; i < pathes.size(); ++i)
    {
        UUID assetDataUUID(pathes[i], UUID::FromStrTag{});
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
        LoadAsset(p);
    }
    // LoadAssets(others);

    for (auto a : validAssetData)
    {
        a->SaveToDisk(projectRoot);
        assetFileSystem.Add(a);
    }
}

void AssetDatabase::RequestShaderRefresh(bool all)
{
    requestShaderRefresh = true;
    requestShaderRefreshAll = all;
}

void AssetDatabase::RefreshShader()
{
    if (requestShaderRefresh)
    {
        requestShaderRefresh = false;
        ShaderLibrary::ReloadAllShaders();
        Material::RebuildAllMaterials();
        requestShaderRefreshAll = false;
    }
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

Asset* AssetDatabase::LoadAsset(std::filesystem::path path, bool forceReimport)
{
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

    // use path relative to AssetDirectory
    if (path.is_absolute())
        return nullptr;

    // find the asset if it's already imported
    auto assetData = assetFileSystem.GetAssetData(path);
    auto absoluteAssetPath = assetDirectory / path;
    // this asset is already imported once, we can read its meta
    if (assetData)
    {
        assetMeta = &assetData->GetMeta();

        // override the asset path because this asset may be an internal asset
        absoluteAssetPath = assetData->GetAssetAbsolutePath();
    }
    if (!std::filesystem::exists(absoluteAssetPath))
        return nullptr;

    std::filesystem::path ext = absoluteAssetPath.extension();
    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());
    if (loader == nullptr)
        return nullptr;
    loader->Setup(importDatabase, absoluteAssetPath, *assetMeta);

    bool importNeeded = forceReimport || loader->ImportNeeded();
    std::vector<std::filesystem::path> importedAssetFilePaths;
    if (importNeeded)
    {
        importedAssetFilePaths = loader->Import();

        if (assetData != nullptr)
        {
            assetFileSystem.SyncImportedAssetFiles(assetData, importedAssetFilePaths);
        }
    }

    Asset* asset = assetData ? assetData->GetAsset() : nullptr;
    bool alreadyLoaded = asset != nullptr;
    bool loadNeeded = importNeeded ? importNeeded : asset == nullptr;

    // no import and load process taken, this asset is ready to be used
    if (!importNeeded && !loadNeeded)
    {
        return asset;
    }

    if (loadNeeded)
    {
        loader->Load();
    }

    std::unique_ptr<Asset> newAsset = loader->RetrieveAsset();

    // failed to load asset
    if (newAsset == nullptr)
    {
        return nullptr;
    }

    asset = newAsset.get();

    // make sure we have the assetData ready
    if (!assetData)
    {
        std::unique_ptr<AssetData> ad = std::make_unique<AssetData>(std::move(newAsset), path, projectRoot);
        assetData = ad.get();

        AddAssetData(std::move(ad));
        assetFileSystem.SyncImportedAssetFiles(assetData, importedAssetFilePaths);
    }
    else
    {
        // this needs to be done after importing becuase if not we don't have internal game object's name to
        // set UUID by SetAsset(implementation detail leakage, refactor may be needed). It also needs to
        // happen before reference resolve so that it has the correct UUID
        asset = assetData->SetAsset(std::move(newAsset), projectRoot);

        // this asset has a aseet data and is already loaded, it's a reload!
        if (alreadyLoaded)
        {
            loader->HandleReload(asset);
        }
    }

    assetData->SetMeta(loader->GetMeta());
    assetData->SaveToDisk(projectRoot);

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

    asset->OnLoaded();
    return asset;
}

void AssetDatabase::CreateFolderAtPath(const std::filesystem::path& path)
{
    int i = -1;
    std::string fileName;
    do
    {
        i++;
        fileName = fmt::format("{}/{} {}", path.string(), "New Folder", i);
    }
    while (std::filesystem::exists(fileName));
    std::filesystem::create_directory(fileName);
}

void AssetDatabase::Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
{
    // TODO: sync async works before accessing assetFileSystem
    assetFileSystem.Rename(oldPath, newPath);
}

void AssetDatabase::Remove(const std::filesystem::path& path)
{
    // TODO: sync async works before accessing assetFileSystem
    AssetData* assetData = assetFileSystem.GetAssetData(path);

    if (assetData)
    {
        assetDatas.erase(
            std::remove_if(assetDatas.begin(), assetDatas.end(), [&](auto& d) { return d.get() == assetData; })
        );
    }

    assetFileSystem.Remove(path);
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
            [assetData](const std::unique_ptr<AssetData>& dd) { return dd.get() == assetData; }
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

// ObjPtr<Asset> AssetDatabase::LoadAssetAsync(const std::filesystem::path& path)
// {
//     const UUID& uuid = GetUUIDFromPath(path);
//
//     if (uuid == UUID::GetEmptyUUID())
//     {
//         return nullptr;
//     }
//
//     return ObjPtr<Asset>(uuid);
// }

const std::filesystem::path& AssetDatabase::GetAssetPath(const UUID& uuid)
{
    auto assetData = assetFileSystem.GetAssetData(uuid);
    if (assetData)
        return assetData->GetAssetPath();

    static std::filesystem::path empty = "";
    return empty;
}

const std::filesystem::path& AssetDatabase::GetAssetDirectory() const
{
    return assetDirectory;
}

const std::vector<AssetData*>& AssetDatabase::GetInternalAssets() const
{
    return internalAssets;
}

std::filesystem::path AssetDatabase::AbsolutePathToAssetPath(const std::filesystem::path& absolutePath)
{
    return std::filesystem::relative(absolutePath, assetDirectory);
}

const std::filesystem::path& AssetDatabase::GetProjectRoot() const
{
    return projectRoot;
}

const std::filesystem::path& AssetDatabase::GetProjectAssetDatabaseDirectory() const
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
