#pragma once
#include "Core/Resource.hpp"
#include <filesystem>
#include <memory>

namespace Engine
{

class AssetMeta
{};

class Asset
{};

template <IsResource T>
class AssetImpl : public Asset
{
public:
    AssetImpl(std::unique_ptr<T>&& resource, const std::filesystem::path& path)
        : resource(std::move(resource)), path(path)
    {}

    void Save() { auto id = SerializableAsset<T>::assetID; }

private:
    std::unique_ptr<Resource> resource;
    std::filesystem::path path;
};
} // namespace Engine
