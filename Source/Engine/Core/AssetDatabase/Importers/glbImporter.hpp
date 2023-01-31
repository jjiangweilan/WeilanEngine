#pragma once
#include "../AssetImporter.hpp"
#include "Core/Model.hpp"
#include <nlohmann/json.hpp>

namespace Engine::Internal
{
class glbImporter : public AssetImporter
{
public:
    virtual ~glbImporter() override{};
    void Import(const std::filesystem::path& path,
                const std::filesystem::path& root,
                const nlohmann::json& json,
                const UUID& rootUUID,
                const std::unordered_map<std::string, UUID>& containedUUIDs) override;

    virtual UniPtr<AssetObject> Load(const std::filesystem::path& root,
                                     ReferenceResolver& refResolver,
                                     const UUID& uuid) override;

    const std::type_info& GetObjectType() override;

private:
};
} // namespace Engine::Internal
