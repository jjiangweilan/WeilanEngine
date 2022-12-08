
#pragma once

#include "Code/Ptr.hpp"
#include "AssetFile.hpp"
#include "AssetImportCache.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
namespace Engine
{
    class AssetImporter
    {
        public:
            virtual ~AssetImporter() {};

            /**
             * @brief 
             * 
             * @param path path to the file that needs to be imported
             * @param uuid assetImporter should use this uuid to create the AssetObject
             * @return UniPtr<AssetObject> 
             */
            virtual UniPtr<AssetObject> Import(
                    const std::filesystem::path& path,
                    RefPtr<AssetImportCache> importCache,
                    const nlohmann::json& json,
                    ReferenceResolver& refResolver,
                    const UUID& rootUUID,
                    const std::unordered_map<std::string, UUID>& containedUUIDs) = 0;

            virtual nlohmann::json GetDefaultConfig() { return nlohmann::json{}; }
        private:
    };
}
