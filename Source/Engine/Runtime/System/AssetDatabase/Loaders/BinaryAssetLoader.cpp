#include "BinaryAssetLoader.hpp"

#include "Engine/Core/BinaryAsset.hpp"
#include <typeindex>

DEFINE_ASSET_LOADER(BinaryAssetLoader, "bin")

const std::vector<std::type_index>& BinaryAssetLoader::GetImportTypes()
{
    static const std::vector<std::type_index> types = {typeid(BinaryAsset)};
    return types;
}

void BinaryAssetLoader::Load()
{
    auto loaded = std::make_unique<BinaryAsset>();
    if (loaded->LoadFromFile(absoluteAssetPath.string().c_str()))
        asset = std::move(loaded);
}

std::unique_ptr<Asset> BinaryAssetLoader::RetrieveAsset()
{
    return std::move(asset);
}
