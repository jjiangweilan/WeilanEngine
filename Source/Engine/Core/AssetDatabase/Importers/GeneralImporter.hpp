#pragma once
#include "../AssetImporter.hpp"
#include "../AssetSerializer.hpp"
namespace Engine::Internal
{
    template<class T>
        class GeneralImporter : public AssetImporter
    {
        void Import(
                const std::filesystem::path& path,
                const std::filesystem::path& root,
                const nlohmann::json& json,
                const UUID& uuid,
                const std::unordered_map<std::string, UUID>& containedUUIDs) override
        {
            auto importPath = root / "Library" / uuid.ToString();
            std::filesystem::copy_file(path, importPath);
        }

        UniPtr<AssetObject> Load(
                const std::filesystem::path& root,
                ReferenceResolver& refResolver,
                const UUID& uuid)
        {
            std::filesystem::path loadPath = root / "Library" / uuid.ToString();

            if (std::filesystem::exists(loadPath))
            {
                auto asset = MakeUnique<T>();
                AssetSerializer deserializer;
                deserializer.LoadFromDisk(loadPath.string());
                asset->Deserialize(deserializer, refResolver);
                return asset;
            }

            return nullptr;
        }

        const std::type_info& GetObjectType() override
        {
            return typeid(T);
        }
    };
}
