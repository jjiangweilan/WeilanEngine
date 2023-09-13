#include "Asset.hpp"

namespace Engine
{
Asset::Asset(std::unique_ptr<Resource>&& resource, const std::filesystem::path path)
    : resource(std::move(resource)), path(path)
{}

Asset::Asset(const std::filesystem::path& path) {}

const AssetMeta& Asset::GetMeta()
{
    return meta;

    void Save();
}

void Asset::Load()
{
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("asset doesn't exit");
    }

    // load the file
    std::fstream f(path, std::ios::binary | std::ios::in);
    if (f.good())
    {}
}
} // namespace Engine
