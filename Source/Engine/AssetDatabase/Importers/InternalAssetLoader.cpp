#include "InternalAssetLoader.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/Material.hpp"
#include <fstream>
#include <typeindex>

DEFINE_ASSET_LOADER(InternalAssetLoader, "mat,scene,prefab,fgraph,renderPipeline")

class Material;
const DynamicArray<std::type_index>& InternalAssetLoader::GetImportTypes()
{
    static DynamicArray<std::type_index> types = {typeid(Material), typeid(Scene), typeid(GameObject)};
    return types;
}

void InternalAssetLoader::Load()
{
    auto ext = absoluteAssetPath.extension();
    asset = AssetRegistry::CreateAssetByExtension(ext.string().c_str());

    if (asset != nullptr)
    {
        std::ifstream f(absoluteAssetPath, std::ios::binary);
        if (f.is_open() && f.good())
        {
            DynamicArray<uint8_t> binary(std::istreambuf_iterator<char>(f), {});
            ser = JsonSerializer(binary, &resolveMap);
            asset->Deserialize(&ser);
        }
    }
}

void InternalAssetLoader::GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap)
{
    serializer = &ser;
    resolveMap = &this->resolveMap;
}
