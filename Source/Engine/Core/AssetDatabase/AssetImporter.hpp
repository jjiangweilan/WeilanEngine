#pragma once

#include "Code/Ptr.hpp"
#include "AssetFile.hpp"
#include <string>
#include <functional>
#include <unordered_map>
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
                    ReferenceResolver& refResolver,
                    const UUID& rootUUID,
                    const std::unordered_map<std::string, UUID>& containedUUIDs) = 0;

            virtual uint32_t GetAssetSize() = 0;

            static int RegisterImporter(
                    const std::string& extension,
                    const std::function<UniPtr<AssetImporter>()>& importerFactory);

            static RefPtr<AssetImporter> GetImporter(const std::string& extension);
        
        private:
            static std::unordered_map<std::string, UniPtr<AssetImporter>>& GetImporterPrototypes();
    };
}
