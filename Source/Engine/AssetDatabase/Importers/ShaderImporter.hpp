#pragma once
#include "AssetImporter.hpp"
class ShaderImporter : public AssetImporter
{
    DECLARE_ASSET_IMPORTER()

public:
    bool ImportNeeded() override { return false; }
    std::vector<std::filesystem::path> Import() override { return {}; }

    static const std::vector<std::type_index>& GetImportTypes();
};
