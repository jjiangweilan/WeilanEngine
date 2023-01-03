#pragma once
#include "../AssetImporter.hpp"
#include "GfxDriver/ShaderModule.hpp"
#include <string>
#include <unordered_map>
namespace Engine::Internal
{
class ShaderImporter : public AssetImporter
{
public:
    void Import(const std::filesystem::path& path, const std::filesystem::path& root, const nlohmann::json& json,
                const UUID& rootUUID, const std::unordered_map<std::string, UUID>& containedUUIDs) override;

    UniPtr<AssetObject> Load(const std::filesystem::path& root, ReferenceResolver& refResolver,
                             const UUID& uuid) override;

    virtual bool NeedReimport(const std::filesystem::path& path, const std::filesystem::path& root,
                              const UUID& uuid) override;

    const std::type_info& GetObjectType() override;

private:
    const char* YAML_FileName = "config.yaml"; // laste write time is used to track original file modification time
    const char* VERT_FileName = "vert.spv";
    const char* FRAG_FileName = "frag.spv";
    const char* DEP_FileName = "dep.json";
};
} // namespace Engine::Internal
