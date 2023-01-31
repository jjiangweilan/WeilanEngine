#pragma once
#include "../AssetImporter.hpp"
#include "../AssetSerializer.hpp"
namespace Engine::Internal
{
template <class T>
class GeneralImporter : public AssetImporter
{
    void Import(const std::filesystem::path& path,
                const std::filesystem::path& root,
                const nlohmann::json& json,
                const UUID& uuid,
                const std::unordered_map<std::string, UUID>& containedUUIDs) override
    {
        auto importPath = root / "Library" / uuid.ToString();
        std::filesystem::copy_file(path, importPath, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::last_write_time(importPath, std::filesystem::last_write_time(path));
    }

    UniPtr<AssetObject> Load(const std::filesystem::path& root, ReferenceResolver& refResolver, const UUID& uuid)
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

    const std::type_info& GetObjectType() override { return typeid(T); }

    bool NeedReimport(const std::filesystem::path& path, const std::filesystem::path& root, const UUID& uuid)
    {
        if (AssetImporter::NeedReimport(path, root, uuid))
            return true;

        auto outputDir = root / "Library" / uuid.ToString();
        auto t0 = GetLastWriteTime(path);
        auto t1 = GetLastWriteTime(outputDir);
        if (!std::filesystem::exists(outputDir) || t0 > t1)
            return true;
        return false;
    };
};
} // namespace Engine::Internal
