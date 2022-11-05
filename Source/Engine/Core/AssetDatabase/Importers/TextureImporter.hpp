#pragma once
#include "../AssetImporter.hpp"
#include "Core/Texture.hpp"

namespace Engine::Internal
{
        class TextureImporter : public AssetImporter
        {
            virtual UniPtr<AssetObject> Import(
                    const std::filesystem::path& path,
                    ReferenceResolver& refResolver,
                    const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs) override;

            private:
        };
}
