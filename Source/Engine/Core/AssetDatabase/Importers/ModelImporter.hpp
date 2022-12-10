#pragma once
#include "../AssetImporter.hpp"

namespace Engine::Internal
{
    class ModelImporter : public AssetImporter
    {
        public:
            ~ModelImporter() override {};

            void Import(
                    const std::filesystem::path& path,
                    const std::filesystem::path& root,
                    const nlohmann::json& json,
                    const UUID& rootUUID,
                    const std::unordered_map<std::string, UUID>& containedUUIDs) override;

            UniPtr<AssetObject> Load(
                    const std::filesystem::path& root,
                    ReferenceResolver& refResolver,
                    const UUID& uuid) override;

            bool NeedReimport(
                    const std::filesystem::path& path,
                    const std::filesystem::path& root,
                    const UUID& uuid) override;
    };
}
