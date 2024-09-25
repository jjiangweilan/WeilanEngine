#include "InternalAssetLoader.hpp"
#include <fstream>

DEFINE_ASSET_LOADER(InternalAssetLoader, "mat,scene,gobj,fgraph")

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
