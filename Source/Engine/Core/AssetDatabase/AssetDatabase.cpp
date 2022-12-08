#include "AssetDatabase.hpp"
#include <fstream>
#include <spdlog/spdlog.h>
#include "AssetImporter.hpp"
#include <nlohmann/json.hpp>
#include "Core/Global/Global.hpp"
#include "GfxDriver/GfxDriver.hpp"
#if GAME_EDITOR
#include "Editor/ProjectManagement/ProjectManagement.hpp"
#endif
namespace Engine
{
    AssetDatabase* AssetDatabase::instance = nullptr;

    AssetDatabase::AssetDatabase(){}

    std::vector<unsigned char> AssetDatabase::ReadAsBinary(const std::string& path)
    {
        std::error_code errorCode;
        uintmax_t fileSize = std::filesystem::file_size(path, errorCode);

        std::vector<unsigned char> fileContent;

        if (errorCode.value() != 0)
        {
            SPDLOG_ERROR("Failed to get file size. {} File path: {}", errorCode.message(), path);
            return fileContent;
        }

        fileContent.resize(fileSize);

        std::ifstream fs;
        fs.open(path, std::ios::in | std::ios::binary);

        fs.read(reinterpret_cast<char*>(&fileContent[0]), fileSize);

        return fileContent;
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
    }

    void AssetDatabase::Reload(RefPtr<AssetFile> target)
    {
        auto path = target->GetFullPath();
        auto ext = path.extension();
        auto importer = GetImporter(ext.string().substr(1));
        std::unordered_map<std::string, UUID> containedUUIDs;
        for(auto& it : target->GetAllContainedAssets())
        {
            containedUUIDs[it->GetName()] = it->GetUUID();
        }
        UniPtr<AssetObject> obj = importer->Import(path, {}, refResolver, target->GetRoot()->GetUUID(), containedUUIDs);
        target->Reload(std::move(obj));
        for(auto& iter : onAssetReloadCallbacks)
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
        for(auto& shader : shaderMap)
        {
            auto iter = assetFiles.find(shader.second->GetUUID());
            if (iter != assetFiles.end())
            {
                if (iter->second->GetLatestWriteTime() != iter->second->GetLastWriteTime())
                {
                    Reload(iter->second);
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

        for(RefPtr<AssetObject> contained : assetFile->GetAllContainedAssets())
        {
            assetObjects[contained->GetUUID()] = contained;
        }
        assetFiles[assetFile->GetRoot()->GetUUID()] = std::move(assetFile);

        return temp;
    }

    RefPtr<AssetDatabase> AssetDatabase::Instance()
    {
        return instance;
    }

    void AssetDatabase::InitSingleton()
    {
        if (instance == nullptr)
            instance = new AssetDatabase();
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

    bool AssetDatabase::GetObjectPath(const UUID& uuid, std::filesystem::path& path)
    {
        auto iter = assetFiles.find(uuid);
        if (iter != assetFiles.end())
        {
            path = iter->second->GetPath();
            return true;
        }

        return false;
    }

    void AssetDatabase::LoadAllAssets()
    {
        Refresh_Internal("Assets", false);
    }

    RefPtr<AssetObject> AssetDatabase::LoadInternal(const std::filesystem::path& path, bool useRelativeBase, const std::filesystem::path& relativeBase)
    {
        if (!std::filesystem::exists(path)) return nullptr;

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
            catch(nlohmann::json::parse_error& ex)
            {
                SPDLOG_ERROR("{} is not parsable as a meta file", metaPath.string());
                isNewExternalAsset = true;
            }
            if (isValidJsonFile)
            {
                if(metaJson.contains("uuid"))
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
                    for(auto it = metaJson["contained"].begin(); it != metaJson["contained"].end(); ++it)
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
            }
        }

        if (isMetaValid)
        {
            auto ext = path.extension();
            if (ext.empty()) return nullptr;
            auto importer = GetImporter(ext.string().substr(1));
            if (importer == nullptr) return nullptr;
            UniPtr<AssetObject> obj = importer->Import(path, {}, refResolver, uuid, containedUUIDs);
            if (obj != nullptr)
            {
                return StoreImported(path, uuid, relativeBase, std::move(obj), useRelativeBase);
            }
        }

        return nullptr;
    }

    RefPtr<AssetObject> AssetDatabase::StoreImported(std::filesystem::path path, UUID uuid, std::filesystem::path relativeBase, UniPtr<AssetObject>&& obj, bool useRelativeBase)
    {
        std::filesystem::path pathStored = path;
        if (!useRelativeBase)
            pathStored = std::filesystem::proximate(path, relativeBase);
        UniPtr<AssetFile> assetFile = MakeUnique<AssetFile>(std::move(obj), pathStored, relativeBase);
        pathToAssetFile[pathStored] = assetFile;
        assetObjects[assetFile->GetRoot()->GetUUID()] = assetFile->GetRoot();
        for(RefPtr<AssetObject> contained : assetFile->GetAllContainedAssets())
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

    int AssetDatabase::RegisterImporter(const std::string& extension, const std::function<UniPtr<AssetImporter>()>& importerFactory)
    {
        auto iter = importerPrototypes.find(extension);
        if (iter != importerPrototypes.end())
        {
            SPDLOG_WARN("can't register different importer with the same extension");
            return 0;
        }
        importerPrototypes[extension] = importerFactory();

        return 0;
    }
}
