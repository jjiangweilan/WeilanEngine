#include "InternalAssetLoader.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include <fstream>
#include <typeindex>

DEFINE_ASSET_LOADER(InternalAssetLoader, "mat,scene,gobj,fgraph")

class Material;
const std::vector<std::type_index>& InternalAssetLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Material), typeid(Scene), typeid(GameObject), typeid(Rendering::FrameGraph::Graph)};
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
            size_t fileSize = std::filesystem::file_size(absoluteAssetPath);
            std::vector<uint8_t> binary(fileSize);
            f.read((char*)binary.data(), fileSize);
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
