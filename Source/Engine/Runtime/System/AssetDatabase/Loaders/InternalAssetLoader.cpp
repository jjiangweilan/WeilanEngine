#include "InternalAssetLoader.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <fstream>
#include <typeindex>

DEFINE_ASSET_LOADER(InternalAssetLoader, "mat,scene,prefab,fgraph,renderPipeline,nav")

class Material;
const std::vector<std::type_index>& InternalAssetLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Material), typeid(Scene), typeid(GameObject), typeid(NavData)};
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
            std::vector<uint8_t> binary(std::istreambuf_iterator<char>(f), {});
            ser = JsonSerializer(binary, &resolveMap);
            asset->Deserialize(&ser);
            // asset->DeserializeByReflection(&ser);
        }
    }
}

void InternalAssetLoader::GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap)
{
    serializer = &ser;
    resolveMap = &this->resolveMap;
}
