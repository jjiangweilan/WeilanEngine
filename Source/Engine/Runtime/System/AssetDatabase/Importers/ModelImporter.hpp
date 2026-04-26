#pragma once
#include "AssetImporter.hpp"

class ModelImporter : public AssetImporter
{
    DECLARE_ASSET_IMPORTER()
public:
    bool ImportNeeded() override;
    std::vector<std::filesystem::path> Import() override;

    static const std::vector<std::type_index>& GetImportTypes();
};
