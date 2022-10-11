#pragma once

#include "ReferenceResolver.hpp"
#include "Code/UUID.hpp"
#include "Code/Ptr.hpp"
#include "AssetFile.hpp"
#include "Core/Graphics/Shader.hpp"
#include <string>
#include <filesystem>
#include <functional>
#include <unordered_map>
namespace Engine
{
    namespace Editor
    {
        class GameEditor;
    }
    class AssetDatabase
    {
        public:
            static RefPtr<AssetDatabase> Instance();
            static void InitSingleton();
            static void Deinit();

            RefPtr<AssetObject> GetAssetObject(const UUID& uuid);
            RefPtr<AssetFile> GetAssetFile(const std::filesystem::path& path);
            RefPtr<Shader> GetShader(const std::string& name);

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
            bool GetObjectPath(const UUID& uuid, std::filesystem::path& path);

            void LoadAllAssets();

#if GAME_EDITOR
            void LoadInternalAssets();
#endif

            // TODO: remove
            std::vector<unsigned char> ReadAsBinary(const std::string& path);
        protected:

            AssetDatabase();
            static AssetDatabase* instance;

            RefPtr<AssetObject> LoadInternal(const std::filesystem::path& path, bool useRelativeBase = false, const std::filesystem::path& relativeBase = "");

            using Path = std::filesystem::path;
            // from: https://en.cppreference.com/w/cpp/filesystem/path/hash_value
            struct PathHash {
                std::size_t operator()(Path const& p) const noexcept {
                    return std::filesystem::hash_value(p);
                }
            };

            std::unordered_map<std::filesystem::path, RefPtr<AssetFile>, PathHash> pathToAssetFile;
            std::unordered_map<UUID, UniPtr<AssetFile>> assetFiles;
            std::unordered_map<UUID, RefPtr<AssetObject>> assetObjects;
            std::unordered_map<std::string, RefPtr<Shader>> shaderMap;
            ReferenceResolver refResolver;

            void Refresh_Internal(const Path& path, bool isEngineInternal);
            void ProcessAssetFile(const Path& path);
    };

    template<class T>
    RefPtr<T> AssetDatabase::Load(const std::filesystem::path& path)
    {
        return static_cast<T*>(LoadInternal(path).Get());
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
