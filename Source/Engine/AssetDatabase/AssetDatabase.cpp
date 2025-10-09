#include "AssetDatabase.hpp"
#include "AssetDatabase/Importers/AssetImporter.hpp"
#include "AssetDatabase/Loaders/AssetLoader.hpp"
#include "Core/Component/GameScript.hpp"
#include "Core/Scene/Scene.hpp"
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

void AssetDatabase::Reimport(const std::filesystem::path& path)
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
    ShaderLibrary::CompileAllDefaultShaders();

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

ObjPtr<Asset> AssetDatabase::LoadAssetAsync_Experimental(std::filesystem::path path, bool forceReimport)
{
    auto loaded = asyncLoadProcessor.AsyncLoadFromPath(path);

    return loaded;
}

Asset* AssetDatabase::LoadAsset(std::filesystem::path path, bool forceReload)
{
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

    // use path relative to AssetDirectory
    if (path.is_absolute())
        return nullptr;

    // find the asset if it's already imported
    auto assetData = assetFileSystem.GetAssetData(path);
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
    auto absolutePath = assetDirectory / path;
    if (std::filesystem::is_directory(absolutePath))
    {
        for(auto iter : std::filesystem::directory_iterator(absolutePath))
        {
            Remove(iter.path());
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
                std::remove_if(assetDatas.begin(), assetDatas.end(), [&](auto& d) { return d.get() == assetData; })
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

Scene* AssetDatabase::LoadScene(const UUID& sceneUUID)
{
    auto assetData = assetFileSystem.GetAssetData(sceneUUID);
    if (assetData)
    {
        auto scene = asyncLoadProcessor.LoadAssetJob(assetData->GetAssetPath(), assetData);
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

// ObjPtr<Asset> AssetDatabase::LoadAssetAsync(const std::filesystem::path& path)
//{
//     ObjPtr<Asset> ptr = asyncLoadProcessor.AsyncLoadFromPath(path);
//
//     return ptr;
// }

const UUID& AssetDatabase::GetUUIDFromPath(const std::filesystem::path& path)
{
    if (AssetData* assetData = assetFileSystem.GetAssetData(path))
    {
        return assetData->GetAssetUUID();
    }

    return UUID::GetEmptyUUID();
}

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

void AssetDatabase::EnsureAllFilesAreImported(const std::filesystem::path& directory)
{
    for (auto const& dirEntry : std::filesystem::directory_iterator{directory})
    {
        if (dirEntry.is_regular_file())
        {
            const auto& path = dirEntry.path();

            auto assetPath = AbsolutePathToAssetPath(path);
            ImportAssetIfNeeded(assetPath, false);
        }
        else if (dirEntry.is_directory())
        {
            EnsureAllFilesAreImported(dirEntry.path());
        }
    }
}

void AssetDatabase::ImportAssetIfNeeded(const std::filesystem::path& path, bool forceReimport)
{
    auto ext = path.extension();
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
    std::vector<std::filesystem::path> importedAssetFilePaths;
    if (importNeeded)
    {
        importedAssetFilePaths = importer->Import();

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
