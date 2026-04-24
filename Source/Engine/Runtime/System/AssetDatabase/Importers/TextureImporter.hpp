#pragma once
#include "AssetImporter.hpp"

class TextureImporter : public AssetImporter
{
    DECLARE_ASSET_IMPORTER()

public:
    std::vector<std::filesystem::path> Import() override;
    bool ImportNeeded() override;

private:
    static const std::vector<std::type_index>& GetImportTypes();
};
