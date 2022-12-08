#pragma once
#include "../AssetImporter.hpp"

namespace Engine::Internal
{
    class ModelImporter : public AssetImporter
    {
        public:
            virtual ~ModelImporter() override {};
            UniPtr<AssetObject> Import(
                const std::filesystem::path& path,
                RefPtr<AssetImportCache> importCache,
                const nlohmann::json& config, ReferenceResolver& refResolver, const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs) override;

        private:
            UniPtr<AssetObject> ReadCache(const UUID& uuid);
    };
}
