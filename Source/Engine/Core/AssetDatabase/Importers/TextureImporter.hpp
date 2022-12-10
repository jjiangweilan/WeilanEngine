#pragma once
#include "../AssetImporter.hpp"
#include "Core/Texture.hpp"

namespace Engine::Internal
{
    class TextureImporter : public AssetImporter
    {
        public:
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

        protected:
            void GenerateMipMap(unsigned char* image, int level);
    };
}
