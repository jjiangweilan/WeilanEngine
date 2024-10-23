#include "LegacyLoader.hpp"
#include "Core/Model.hpp"

DEFINE_ASSET_LOADER(LegacyLoader, "glb")

const std::vector<std::type_index>& LegacyLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}

void LegacyLoader::Load()
{
    auto ext = absoluteAssetPath.extension();
    asset = std::make_unique<Model>();
    asset->LoadFromFile(absoluteAssetPath.string().c_str());
}
