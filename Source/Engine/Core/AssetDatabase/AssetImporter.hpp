
#pragma once

#include "AssetFile.hpp"
#include "Libs/FileSystem/FileStat.hpp"
#include "Libs/Ptr.hpp"
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <typeinfo>
#include <unordered_map>
namespace Engine
{
class AssetImporter
{
public:
    enum class ImporterResult
    {
        Success,
        NoSuchFile
    };

    virtual ~AssetImporter(){};

    virtual void Import(const std::filesystem::path& path,
                        const std::filesystem::path& root, // root path of the project
                        const nlohmann::json& config,
                        const UUID& rootUUID,
                        const std::unordered_map<std::string, UUID>& containedUUIDs) = 0;

    virtual UniPtr<AssetObject> Load(const std::filesystem::path& root,
                                     ReferenceResolver& refResolver,
                                     const UUID& uuid) = 0;

    virtual bool NeedReimport(const std::filesystem::path& path, const std::filesystem::path& root, const UUID& uuid)
    {
        auto outputDir = root / "Library" / uuid.ToString();
        if (!std::filesystem::exists(outputDir))
            return true;

        return false;
    };

    virtual const std::type_info& GetObjectType() = 0;

    virtual const nlohmann::json& GetDefaultConfig()
    {
        static nlohmann::json j = nlohmann::json::object();
        return j;
    }

protected:
    static std::time_t GetLastWriteTime(const std::filesystem::path& path)
    {
        return Libs::FileSystem::GetLastModifiedTime(path);
    }

private:
};
} // namespace Engine
