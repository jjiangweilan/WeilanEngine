#pragma once
#include "../AssetImporter.hpp"
#include "Core/Model.hpp"
#include <nlohmann/json.hpp>

namespace Engine::Internal
{
    class glbImporter : public AssetImporter
    {
        public:
            virtual ~glbImporter() override {};
            UniPtr<AssetObject> Import(
                const std::filesystem::path& path,
                RefPtr<AssetImportCache> importCache,
                const nlohmann::json& config, ReferenceResolver& refResolver, const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs) override;

        private:
    };
}
