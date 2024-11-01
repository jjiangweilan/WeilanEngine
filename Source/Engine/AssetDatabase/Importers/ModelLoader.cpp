#include "ModelLoader.hpp"
#include "Core/Model.hpp"

DEFINE_ASSET_LOADER(ModelLoader, "glb")

const std::vector<std::type_index>& ModelLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}

void ModelLoader::Load()
{
    auto ext = absoluteAssetPath.extension();
    asset = std::make_unique<Model>();
    asset->LoadFromFile(absoluteAssetPath.string().c_str());
}
