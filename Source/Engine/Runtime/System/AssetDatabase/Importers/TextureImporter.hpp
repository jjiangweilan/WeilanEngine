#pragma once
#include "Engine/WeilanEngineAPI.hpp"
#include "AssetImporter.hpp"

class TextureImporter : public AssetImporter
{
    DECLARE_ASSET_IMPORTER()

public:
    WEILAN_ENGINE_API TextureImporter();
    WEILAN_ENGINE_API ~TextureImporter() override;
    WEILAN_ENGINE_API std::vector<std::filesystem::path> Import() override;
    WEILAN_ENGINE_API bool ImportNeeded() override;

private:
    static const std::vector<std::type_index>& GetImportTypes();
};
