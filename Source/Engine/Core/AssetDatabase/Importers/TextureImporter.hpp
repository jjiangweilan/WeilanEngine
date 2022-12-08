#pragma once
#include "../AssetImporter.hpp"
#include "Core/Texture.hpp"

namespace Engine::Internal
{
        class TextureImporter : public AssetImporter
        {
            virtual UniPtr<AssetObject> Import(
                    const std::filesystem::path& path,
                    const nlohmann::json& json,
                    ReferenceResolver& refResolver,
                    const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs) override;

            virtual nlohmann::json GetDefaultConfig() override;

            private:
        };
}
