#include "InternalAssetImporter.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Runtime/Object/Texture/CursorAtlas.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

DEFINE_ASSET_IMPORTER(InternalAssetImporter, "mat,scene,prefab,fgraph,renderPipeline,nav,cursorAtlas,bin")

const std::vector<std::type_index>& InternalAssetImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {
        typeid(Material), typeid(Scene), typeid(GameObject), typeid(CursorAtlas), typeid(BinaryAsset)
    };
    return types;
}
