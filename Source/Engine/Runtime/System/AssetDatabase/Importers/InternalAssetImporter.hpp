#pragma once
#include "AssetImporter.hpp"

class InternalAssetImporter : public AssetImporter
{
    DECLARE_ASSET_IMPORTER()

public:
    bool ImportNeeded() override { return false; };
    bool IsInternalAsset() override { return true; }
    std::vector<std::filesystem::path> Import() override { return {}; }

    static const std::vector<std::type_index>& GetImportTypes();
};
