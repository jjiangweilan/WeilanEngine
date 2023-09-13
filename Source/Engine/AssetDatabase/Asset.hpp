#pragma once
#include "Core/Resource.hpp"
#include "Libs/Serialization/BinarySerializer.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

namespace Engine
{

struct AssetMeta
{
    // the resource's type id
    ResourceTypeID resourceTypeID;

    // this is the path to the resource the asset linked to
    std::filesystem::path resourcePath;
};
// an asset represent an imported Resource, it contains meta information about the resource and the resource itself
class Asset
{
public:
    // this is used when saving an asset
    Asset(std::unique_ptr<Resource>&& resource, const std::filesystem::path);

    // this is used when loading an asset
    Asset(const std::filesystem::path& path);

    void Save();
    void Load();

    const AssetMeta& GetMeta();

    Resource* GetResource()
    {
        return resource.get();
    }
    const std::filesystem::path& GetPath()
    {
        return path;
    }

private:
    // scaii code stands for Wei Lan Engine Asset
    static const uint32_t WLEA = 0b01010111 << 24 | 0b01001100 << 16 | 0b01000101 << 8 | 0b01000001;
    AssetMeta meta;

    std::unique_ptr<Resource> resource;
    std::filesystem::path path;

    void Serialize(Serializer* ser)
    {
        ser->Serialize("resourceTypeID", meta.resourceTypeID);
        ser->Serialize("resourcePath", meta.resourcePath.string());
        ser->Serialize("resource", resource);
    }
    void Deserialize(Serializer* ser)
    {
        ser->Deserialize("resourceTypeID", meta.resourceTypeID);
        std::string resourcePath;
        ser->Deserialize("resourcePath", resourcePath);
        meta.resourcePath = resourcePath;

        resource = ResourceRegistry::CreateAsset(meta.resourceTypeID);
        ser->Deserialize("resource", resource);
    }
};
} // namespace Engine
