#include "InternalAssetImporter.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/Material.hpp"

DEFINE_ASSET_IMPORTER(InternalAssetImporter, "mat,scene,prefab,fgraph,renderPipeline")

const std::vector<std::type_index>& InternalAssetImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Material), typeid(Scene), typeid(GameObject)};
    return types;
}
