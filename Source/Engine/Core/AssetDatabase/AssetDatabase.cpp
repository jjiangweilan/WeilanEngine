#include "AssetDatabase.hpp"
#include "AssetImporter.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#if GAME_EDITOR
#include "Editor/ProjectManagement/ProjectManagement.hpp"
#endif
namespace Engine
{
AssetDatabase* AssetDatabase::instance = nullptr;

AssetDatabase::AssetDatabase(const std::filesystem::path& root)
    : root(root), ImportConfigPath(root / "Library/AssetDatabase/importConfig.json")
{
    if (!std::filesystem::exists(root / "Library/AssetDatabase"))
    {
        std::filesystem::create_directory(root / "Library/AssetDatabase");
        std::ofstream outImportConfigFile(ImportConfigPath);
        outImportConfigFile << "{}";
    }

    std::ifstream importConfigFile(ImportConfigPath);
    importConfig = nlohmann::json::parse(importConfigFile);
}

AssetDatabase::~AssetDatabase()
{
    std::ofstream configOut(root / "Library/AssetDatabase/importConfig.json");
    configOut << importConfig.dump();
}

void AssetDatabase::Refresh_Internal(const std::filesystem::path& path, bool isEngineInternal)
{
    for (const auto& dirEntry : std::filesystem::directory_iterator(path))
    {
        auto ext = dirEntry.path().extension();
        if (dirEntry.is_directory())
        {
            Refresh_Internal(dirEntry, isEngineInternal);
        }
        else if (ext != ".meta")
        {
            auto asset = LoadInternal(dirEntry, isEngineInternal, Editor::GetSysConfigPath());

            if (ext == ".shad")
            {
                if (asset != nullptr)
                    shaderMap[asset->GetName()] = asset;
                else
                    SPDLOG_WARN("Shader loading failed, {}", dirEntry.path().string());
            }
        }
    }
}

void AssetDatabase::SaveAll()
{
    for (auto& iter : assetFiles)
    {
        iter.second->Save();
    }

    std::ofstream configOut(root / "Library/AssetDatabase/importConfig.json");
    configOut << importConfig.dump();
}

void AssetDatabase::Reimport(const UUID& uuid, const nlohmann::json& config, bool force)
{
    auto iter = assetFiles.find(uuid);
    if (iter != assetFiles.end())
    {
        Reimport(iter->second, config, force);
    }
}

void AssetDatabase::Reimport(RefPtr<AssetFile> target, const nlohmann::json& config, bool force)
{
    auto path = target->GetFullPath();
    auto ext = path.extension();
    auto importer = GetImporter(ext.string().substr(1));
    std::unordered_map<std::string, UUID> containedUUIDs;
    for (auto& it : target->GetAllContainedAssets())
    {
        containedUUIDs[it->GetName()] = it->GetUUID();
    }
    if (force || importer->NeedReimport(path, root, target->GetRoot()->GetUUID()) ||
        importConfig[target->GetRoot()->GetUUID().ToString()] != config)
    {
        if (!config.empty())
        {
            importer->Import(path, root, config, target->GetRoot()->GetUUID(), containedUUIDs);
            importConfig[target->GetRoot()->GetUUID().ToString()] = config;
        }
        else
        {
            importer->Import(path, root, importer->GetDefaultConfig(), target->GetRoot()->GetUUID(), containedUUIDs);
            importConfig[target->GetRoot()->GetUUID().ToString()] = importer->GetDefaultConfig();
        }
    }
    UniPtr<AssetObject> obj = importer->Load(root, refResolver, target->GetRoot()->GetUUID());
    target->Reload(std::move(obj));
    for (auto& iter : onAssetReloadCallbacks)
    {
        iter(target->GetRoot());
    }
}

void AssetDatabase::EndOfFrameUpdate()
{
    if (reloadShader)
    {
        Gfx::GfxDriver::Instance()->WaitForIdle();
        ReloadShadersImpl();
        reloadShader = false;
    }
}

void AssetDatabase::ReloadShadersImpl()
{
    for (auto& shader : shaderMap)
    {
        auto iter = assetFiles.find(shader.second->GetUUID());
        if (iter != assetFiles.end())
        {
            if (iter->second->GetLatestWriteTime() != iter->second->GetLastWriteTime())
            {
                Reimport(iter->second);
            }
        }
    }
}

RefPtr<Shader> AssetDatabase::GetShader(const std::string& name)
{
    auto iter = shaderMap.find(name);
    if (iter != shaderMap.end())
    {
        return iter->second;
    }

    return nullptr;
}

RefPtr<AssetObject> AssetDatabase::Save(UniPtr<AssetObject>&& assetObject, const std::filesystem::path& path)
{
    UniPtr<AssetFile> assetFile = MakeUnique<AssetFile>(std::move(assetObject), path, std::filesystem::current_path());

    RefPtr<AssetObject> temp = assetFile->GetRoot();
    assetObjects[temp->GetUUID()] = temp;
    assetFile->Save();

    for (RefPtr<AssetObject> contained : assetFile->GetAllContainedAssets())
    {
        assetObjects[contained->GetUUID()] = contained;
    }
    assetFiles[assetFile->GetRoot()->GetUUID()] = std::move(assetFile);

    return temp;
}

RefPtr<AssetDatabase> AssetDatabase::Instance() { return instance; }

void AssetDatabase::InitSingleton(const std::filesystem::path& root)
{
    if (instance == nullptr)
        instance = new AssetDatabase(root);
}

void AssetDatabase::Deinit()
{
    if (instance != nullptr)
    {
        delete instance;
        instance = nullptr;
    }
}

RefPtr<AssetObject> AssetDatabase::GetAssetObject(const UUID& uuid)
{
    auto iter = assetObjects.find(uuid);
    if (iter != assetObjects.end())
    {
        return iter->second;
    }

    return nullptr;
}

#if GAME_EDITOR
void AssetDatabase::LoadInternalAssets()
{
    Refresh_Internal(Editor::ProjectManagement::instance->GetInternalRootPath() / "Assets", true);
}
#endif

const std::filesystem::path& AssetDatabase::GetObjectPath(const UUID& uuid)
{
    auto iter = assetFiles.find(uuid);
    if (iter != assetFiles.end())
    {
        return iter->second->GetPath();
    }

    static std::filesystem::path emptyPath;
    return emptyPath;
}

void AssetDatabase::LoadAllAssets() { Refresh_Internal("Assets", false); }

RefPtr<AssetObject> AssetDatabase::LoadInternal(const std::filesystem::path& path,
                                                bool useRelativeBase,
                                                const std::filesystem::path& relativeBase)
{
    if (!std::filesystem::exists(path))
        return nullptr;

    // try load the meta file
    auto metaPath = path;
    metaPath.replace_extension(path.extension().string() + ".meta");
    bool isNewExternalAsset = false;
    bool isMetaValid = false;
    UUID uuid;
    std::unordered_map<std::string, UUID> containedUUIDs;
    if (std::filesystem::is_regular_file(metaPath))
    {
        // try to read from the meta file
        std::ifstream inputFile(metaPath);
        bool isValidJsonFile = false;
        nlohmann::json metaJson;
        try
        {
            metaJson = nlohmann::json::parse(inputFile);
            isValidJsonFile = true;
        }
        catch (nlohmann::json::parse_error& ex)
        {
            SPDLOG_ERROR("{} is not parsable as a meta file", metaPath.string());
            isNewExternalAsset = true;
        }
        if (isValidJsonFile)
        {
            if (metaJson.contains("uuid"))
            {
                std::string uuidStr = metaJson["uuid"];
                uuid = UUID(uuidStr);
                isMetaValid = true;
            }
            else
            {
                isNewExternalAsset = true;
            }

            if (metaJson.contains("contained"))
            {
                for (auto it = metaJson["contained"].begin(); it != metaJson["contained"].end(); ++it)
                {
                    containedUUIDs[it.key()] = UUID(it->get<std::string>());
                }
            }
        }
    }
    else
    {
        isNewExternalAsset = true;
    }

    // see if this asset already loaded
    auto iter = assetFiles.find(uuid);
    if (iter != assetFiles.end())
    {
        return iter->second->GetRoot();
    }

    auto ext = path.extension();
    if (ext.empty())
        return nullptr;
    auto importer = GetImporter(ext.string().substr(1));
    if (importer == nullptr)
        return nullptr;

    // if this is a new external asset, write the meta to disk
    if (isNewExternalAsset)
    {
        std::ofstream metaFileOutput(metaPath);

        if (metaFileOutput.good())
        {
            nlohmann::json j;
            j["uuid"] = uuid.ToString();
            metaFileOutput << j.dump();
            isMetaValid = true;
            auto config = importer->GetDefaultConfig();
            importer->Import(path, root, config, uuid, containedUUIDs);
            importConfig[uuid.ToString()] = config;
        }
    }
    else if (importer->NeedReimport(path, root, uuid))
    {
        auto config = importConfig.value(uuid.ToString(), importer->GetDefaultConfig());
        importer->Import(path, root, config, uuid, containedUUIDs);
        importConfig[uuid.ToString()] = config;
    }

    if (isMetaValid)
    {
        UniPtr<AssetObject> obj = importer->Load(root, refResolver, uuid);
        if (obj != nullptr)
        {
            return StoreImported(path, uuid, relativeBase, std::move(obj), useRelativeBase);
        }
    }

    return nullptr;
}

RefPtr<AssetObject> AssetDatabase::StoreImported(std::filesystem::path path,
                                                 UUID uuid,
                                                 std::filesystem::path relativeBase,
                                                 UniPtr<AssetObject>&& obj,
                                                 bool useRelativeBase)
{
    if (path.string().find("Monkey") != std::string::npos)
    {
        int i = 0;
    }
    std::filesystem::path pathStored = path;
    if (!useRelativeBase)
        pathStored = std::filesystem::proximate(path, relativeBase);
    UniPtr<AssetFile> assetFile = MakeUnique<AssetFile>(std::move(obj), pathStored, relativeBase);
    pathToAssetFile[pathStored] = assetFile;
    assetObjects[assetFile->GetRoot()->GetUUID()] = assetFile->GetRoot();
    for (RefPtr<AssetObject> contained : assetFile->GetAllContainedAssets())
    {
        assetObjects[contained->GetUUID()] = contained;
    }
    AssetFile* temp = assetFile.Get();
    assetFiles[uuid] = std::move(assetFile);
    refResolver.ResolveAllPending(*this);
    return temp->GetRoot();
}

RefPtr<AssetFile> AssetDatabase::GetAssetFile(const std::filesystem::path& path)
{
    auto iter = pathToAssetFile.find(path);
    if (iter == pathToAssetFile.end())
    {
        return nullptr;
    }

    return iter->second;
}

nlohmann::json AssetDatabase::GetImporterConfig(const UUID& uuid, const std::type_index& fallBackType)
{
    auto configIter = importConfig.find(uuid.ToString());
    if (configIter != importConfig.end())
    {
        return configIter.value();
    }

    auto importerIter = importersTypeMap.find(fallBackType);
    if (importerIter != importersTypeMap.end())
    {
        return importerIter->second->GetDefaultConfig();
    }

    return nlohmann::json(nullptr);
}
} // namespace Engine
