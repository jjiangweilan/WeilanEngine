#pragma once
#include "../AssetImporter.hpp"
#include "GfxDriver/ShaderModule.hpp"
#include <unordered_map>
#include <string>
namespace Engine::Internal
{
    class ShaderImporter : public AssetImporter
    {
        public:
            void Import(
                    const std::filesystem::path& path,
                    const std::filesystem::path& root,
                    const nlohmann::json& json,
                    const UUID& rootUUID,
                    const std::unordered_map<std::string, UUID>& containedUUIDs);

            UniPtr<AssetObject> Load(
                    const std::filesystem::path& root,
                    ReferenceResolver& refResolver,
                    const UUID& uuid);

            virtual bool NeedReimport(
                const std::filesystem::path& path,
                const std::filesystem::path& root,
                const UUID& uuid);

        private:

            const char* YAML_FileName = "config.yaml";
            const char* VERT_FileName = "vert.spv";
            const char* FRAG_FileName = "frag.spv";
            const char* DEP_FileName = "dep.json";
    };
}
