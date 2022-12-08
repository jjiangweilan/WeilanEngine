#pragma once
#include "../AssetImporter.hpp"
#include "GfxDriver/ShaderModule.hpp"
#include <unordered_map>
#include <string>
namespace Engine::Internal
{
    class ShaderImporter : public AssetImporter
    {
        virtual UniPtr<AssetObject> Import(
                const std::filesystem::path& path,
                const nlohmann::json& json,
                ReferenceResolver& refResolver,
                const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs) override;

        private:
    };
}
