#include "AssetDatabase.hpp"
namespace Engine
{
Asset* AssetDatabase::LoadAsset(const std::filesystem::path& path)
{
    // find the asset if it's already loaded
    auto assetPathStr = path.string();
    auto assetsIter = assets.find(assetPathStr);
    if (assetsIter != assets.end())
    {
        return assetsIter->second.get();
    }

    // read the file
    std::unique_ptr<Asset> asset = std::make_unique<Asset>(path);
    asset->GetMeta();

    std::ifstream in;
    in.open(path, std::ios_base::in | std::ios_base::binary);
    nlohmann::json j;
    if (in.is_open() && in.good())
    {
        size_t size = std::filesystem::file_size(path);
        if (size != 0)
        {
            std::string json;
            in >> json;
            j = nlohmann::json::parse(json);
        }
    }
    else
        return nullptr;

    // deserialize
    SerializeReferenceResolveMap resolveMap;
    JsonSerializer ser(j, &resolveMap);
    std::unique_ptr newAsset = std::make_unique<Asset>(path);
    // newAsset->Deserialize(&ser);

    // TODO: resolve reference

    // return
    auto tmpPtr = newAsset.get();
    assets[assetPathStr] = std::move(newAsset);
    return tmpPtr;
}

} // namespace Engine
