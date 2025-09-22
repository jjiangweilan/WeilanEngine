#include "AssetLoader.hpp"
#include <fstream>
#include <iostream>
PodVector<uint8_t> ImportDatabase::ReadFile(const std::string& filename)
{
    std::ifstream f;
    auto absoluteAssetPath = importDatabaseRoot / filename;
    f.rdbuf()->pubsetbuf(streamBuf.data(), streamBufSize);
    f.open(absoluteAssetPath, std::ios::binary);
    if (!f.good())
        return {};
    auto fileSize = std::filesystem::file_size(absoluteAssetPath);
    PodVector<uint8_t> d(fileSize);
    f.read((char*)d.data(), fileSize);
    return d;
}

std::filesystem::path ImportDatabase::GetImportAssetPath(const std::string& filename)
{
    return importDatabaseRoot / filename;
}

std::unique_ptr<AssetLoader> AssetLoaderRegistry::CreateAssetLoaderByExtension(const Extension& id)
{
    auto iter = GetAssetLoaderExtensionRegistry()->find(id);
    if (iter != GetAssetLoaderExtensionRegistry()->end())
    {
        return iter->second();
    }

    return nullptr;
}
char AssetLoaderRegistry::RegisterAssetLoader(
    const std::vector<std::string>& exts, const Creator& creator, const std::vector<std::type_index>& types
)
{
    for (auto& e : exts)
    {
        GetAssetLoaderExtensionRegistry()->emplace(e, creator);
    }

    for (auto& t : types)
    {
        GetAssetLoaderTypeRegistry()->emplace(t, creator);
    }
    return '0';
}

std::unordered_map<std::type_index, std::function<std::unique_ptr<AssetLoader>()>>* AssetLoaderRegistry::
    GetAssetLoaderTypeRegistry()
{
    static std::unordered_map<std::type_index, AssetLoaderRegistry::Creator> registeredAssetLoader =
        std::unordered_map<std::type_index, AssetLoaderRegistry::Creator>();
    return &registeredAssetLoader;
}

std::unordered_map<AssetLoaderRegistry::Extension, std::function<std::unique_ptr<AssetLoader>()>>* AssetLoaderRegistry::
    GetAssetLoaderExtensionRegistry()
{
    static std::unordered_map<Extension, AssetLoaderRegistry::Creator> registeredAssetLoader =
        std::unordered_map<Extension, AssetLoaderRegistry::Creator>();
    return &registeredAssetLoader;
}

std::unique_ptr<AssetLoader> AssetLoaderRegistry::CreateAssetLoaderByType(const std::type_info& type)
{
    auto iter = GetAssetLoaderTypeRegistry()->find(type);
    if (iter != GetAssetLoaderTypeRegistry()->end())
    {
        return iter->second();
    }

    return nullptr;
}
