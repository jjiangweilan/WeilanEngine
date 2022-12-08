#pragma once
#include "../AssetImporter.hpp"

namespace Engine::Internal
{
    class LuaImporter : public AssetImporter
    {
        public:
            virtual ~LuaImporter() override {};
            UniPtr<AssetObject> Import(const std::filesystem::path& path, const nlohmann::json& config, ReferenceResolver& refResolver, const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs) override;
    };
}
