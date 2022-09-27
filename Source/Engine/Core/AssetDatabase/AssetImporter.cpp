#include "AssetImporter.hpp"

namespace Engine
{
    std::unordered_map<std::string, UniPtr<AssetImporter>>& AssetImporter::GetImporterPrototypes()
    {
        static std::unordered_map<std::string, UniPtr<AssetImporter>> importerPrototypes;
        return importerPrototypes;
    }

    int AssetImporter::RegisterImporter(const std::string& extension, const std::function<UniPtr<AssetImporter>()>& importerFactory)
    {
        auto iter = GetImporterPrototypes().find(extension);
        if (iter != GetImporterPrototypes().end())
        {
            SPDLOG_WARN("can't register different importer with the same extension");
            return 0;
        }
        GetImporterPrototypes()[extension] = importerFactory();

        return 0;
    }

    RefPtr<AssetImporter> AssetImporter::GetImporter(const std::string& extension)
    {
        return GetImporterPrototypes()[extension];
    }

}
