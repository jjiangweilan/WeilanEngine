#pragma once
#include "../AssetImporter.hpp"
#include "../AssetSerializer.hpp"
namespace Engine::Internal
{
    template<class T>
    class GeneralImporter : public AssetImporter
    {
        UniPtr<AssetObject> Import(const std::filesystem::path& path,
                                const nlohmann::json& json,
                                   ReferenceResolver& refResolver,
                                   const UUID& uuid,
                                   const std::unordered_map<std::string, UUID>& containedUUIDs) override
        {
            auto asset = MakeUnique<T>();
            AssetSerializer deserializer;
            deserializer.LoadFromDisk(path.string());
            asset->Deserialize(deserializer, refResolver);
            return asset;
        }
    };
}
