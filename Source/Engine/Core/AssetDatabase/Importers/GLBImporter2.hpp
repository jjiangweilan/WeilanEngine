#pragma once
#include "../AssetImporter.hpp"
#include "Core/Model2.hpp"
#include <nlohmann/json.hpp>

namespace Engine::Internal
{
class GLBImporter2 : public AssetImporter
{
public:
    virtual ~GLBImporter2() override{};
    void Import(const std::filesystem::path& path,
                const std::filesystem::path& root,
                const nlohmann::json& json,
                const UUID& rootUUID,
                const std::unordered_map<std::string, UUID>& containedUUIDs) override;

    virtual UniPtr<AssetObject> Load(const std::filesystem::path& root,
                                     ReferenceResolver& refResolver,
                                     const UUID& uuid) override;

    const std::type_info& GetObjectType() override { return typeid(Model2); }

private:
};
} // namespace Engine::Internal
