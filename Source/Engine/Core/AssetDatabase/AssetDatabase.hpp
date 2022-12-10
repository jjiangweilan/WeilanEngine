#pragma once

#include "ReferenceResolver.hpp"
#include "Code/UUID.hpp"
#include "Code/Ptr.hpp"
#include "AssetFile.hpp"
#include "DirectoryNode.hpp"
#include "Core/Graphics/Shader.hpp"
#include "AssetImporter.hpp"
#include <list>
#include <string>
#include <filesystem>
#include <list>
#include <functional>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <nlohmann/json.hpp>
namespace Engine
{
    class AssetDatabase
    {
        public:
            ~AssetDatabase();
            using OnAssetReload = std::function<void(RefPtr<AssetObject>)>;
            using OnAssetReloadIterHandle = std::list<OnAssetReload>::iterator;
            enum class ImportAssetResult
            {
                Success, ImportFailed ,ExtensionNotSupported
            };

            static RefPtr<AssetDatabase> Instance();
            static void InitSingleton(const std::filesystem::path& root);
            static void Deinit();

            RefPtr<AssetObject> GetAssetObject(const UUID& uuid);
            RefPtr<AssetFile> GetAssetFile(const std::filesystem::path& path);
            RefPtr<Shader> GetShader(const std::string& name);
            OnAssetReloadIterHandle RegisterOnAssetReload(const OnAssetReload& callback) { return onAssetReloadCallbacks.insert(onAssetReloadCallbacks.end(), callback);}
            void UnregisterOnAssetReload(OnAssetReloadIterHandle handle) { onAssetReloadCallbacks.erase(handle); }
            void EndOfFrameUpdate();

            const DirectoryNode& GetDbRoot() { return databaseRoot; }

            /**
             * @brief Save an AssetObject to disk
             * 
             * @param assetObject 
             * @param directory the target directory
             * @param name the name of the file without extension
             * @return RefPtr<AssetObject> 
             */
            RefPtr<AssetObject> Save(UniPtr<AssetObject>&& assetObject, const std::filesystem::path& path);
            void SaveAll();
            template<class T = AssetObject>
                RefPtr<T> Load(const std::filesystem::path& path);
            const std::filesystem::path& GetObjectPath(const UUID& uuid);
            void ReloadShaders() { reloadShader = true; }
            void LoadAllAssets();
            void Reimport(const UUID& uuid, const nlohmann::json& config = nlohmann::json::object(), bool force = false);
            void Reimport(RefPtr<AssetFile> target, const nlohmann::json& config = nlohmann::json::object(), bool force = false);
            template<class ImporterType>
            int RegisterImporter(const std::string& extension);
            nlohmann::json GetImporterConfig(const UUID& uuid, const std::type_index& fallBackType);

#if GAME_EDITOR
            void LoadInternalAssets();
#endif

        protected:
            using Path = std::filesystem::path;
            // from: https://en.cppreference.com/w/cpp/filesystem/path/hash_value
            struct PathHash {
                std::size_t operator()(Path const& p) const noexcept {
                    return std::filesystem::hash_value(p);
                }
            };
            AssetDatabase(const std::filesystem::path& root);
            RefPtr<AssetObject> LoadInternal(
                    const std::filesystem::path& path,
                    bool useRelativeBase = false,
                    const std::filesystem::path& relativeBase = "");
            void Refresh_Internal(const Path& path, bool isEngineInternal);
            void ProcessAssetFile(const Path& path);
            RefPtr<AssetObject> StoreImported(std::filesystem::path path, UUID uuid, std::filesystem::path relativeBase, UniPtr<AssetObject>&& obj, bool useRelativeBase);
            void ReloadShadersImpl();

            RefPtr<AssetImporter> GetImporter(const std::string& extension) { return importerExtMap[extension]; } 
            template<class T>
            RefPtr<AssetImporter> GetImporter() { return importersTypeMap[typeid(T)]; }
            RefPtr<AssetImporter> GetImporter(const std::type_index& typeIndex) { return importersTypeMap[typeIndex]; }

            static AssetDatabase* instance;
            const std::filesystem::path root;
            const std::filesystem::path ImportConfigPath;
            DirectoryNode databaseRoot;
            std::list<DirectoryNode> directories;
            std::list<OnAssetReload> onAssetReloadCallbacks;
            std::unordered_map<std::filesystem::path, RefPtr<AssetFile>, PathHash> pathToAssetFile;
            std::unordered_map<UUID, UniPtr<AssetFile>> assetFiles;
            std::unordered_map<UUID, RefPtr<AssetObject>> assetObjects;
            std::unordered_map<std::string, RefPtr<Shader>> shaderMap;
            std::unordered_map<std::string, UniPtr<AssetImporter>> importerExtMap;
            std::unordered_map<std::type_index, RefPtr<AssetImporter>> importersTypeMap;
            nlohmann::json importConfig;
            ReferenceResolver refResolver;
            bool reloadShader = false;
    };

    template<class T>
    RefPtr<T> AssetDatabase::Load(const std::filesystem::path& path)
    {
        return static_cast<T*>(LoadInternal(path).Get());
    }

    template<class T>
    int AssetDatabase::RegisterImporter(const std::string& extension)
    {
        auto iter = importerExtMap.find(extension);
        if (iter != importerExtMap.end())
        {
            SPDLOG_WARN("can't register different importer with the same extension");
            return 0;
        }
        auto imp = MakeUnique<T>();
        auto temp = imp.Get();
        importerExtMap[extension] = std::move(imp);
        importersTypeMap[typeid(T)] = temp;

        return 0;
    }
}

/*
   Asset System:

   1. Serialization
   Inherit from AssetObject to make an object Serializable.
   Use EDITABLE to make members in the AssetObject's derived class be considered as serialized fields. To make a type serializable, define the function SerializerGetTypeSize and SerializerWriteToMem in member's type's namespace.
   When the member is of type RefPtr<T> or T*, and T is a AssetObject, the default serializing function store T's UUID so that when the serialized data is imported, the reference can be resolved.
   When the member is UniPtr<T>, the default serializing function serialize the content of T.

   2. Import
   All the serialized asset or third party assets(e.g. .glb file) are first imported to the engine.
   The import process are as follow:
   class types that supports 
   */
