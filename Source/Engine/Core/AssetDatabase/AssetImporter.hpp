
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
            enum class ImporterResult
            {
                Success, NoSuchFile
            };

            virtual ~AssetImporter() {};

            virtual void Import(
                    const std::filesystem::path& path,
                    const std::filesystem::path& root, // root path of the project
                    const nlohmann::json& config,
                    const UUID& rootUUID,
                    const std::unordered_map<std::string, UUID>& containedUUIDs) = 0;

            virtual UniPtr<AssetObject> Load(
                    const std::filesystem::path& root,
                    ReferenceResolver& refResolver,
                    const UUID& uuid) = 0;

            virtual bool NeedReimport(
                    const std::filesystem::path& path,
                    const std::filesystem::path& root,
                    const UUID& uuid) {
                auto outputDir = root / "Library" / uuid.ToString();
                if (!std::filesystem::exists(outputDir)) return true;
                return false;
            };

            virtual nlohmann::json GetDefaultConfig() { return nlohmann::json{}; }
        private:
    };
}
