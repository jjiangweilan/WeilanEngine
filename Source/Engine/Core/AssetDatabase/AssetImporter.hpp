
#pragma once

#include "Code/Ptr.hpp"
#include "AssetFile.hpp"
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
                    const nlohmann::json& json,
                    ReferenceResolver& refResolver,
                    const UUID& rootUUID,
                    const std::unordered_map<std::string, UUID>& containedUUIDs) = 0;

            virtual nlohmann::json GetDefaultConfig() { return nlohmann::json{}; }

            static int RegisterImporter(
                    const std::string& extension,
                    const std::function<UniPtr<AssetImporter>()>& importerFactory);

            static RefPtr<AssetImporter> GetImporter(const std::string& extension);
        
        private:
            static std::unordered_map<std::string, UniPtr<AssetImporter>>& GetImporterPrototypes();
    };
}
