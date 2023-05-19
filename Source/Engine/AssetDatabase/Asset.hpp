#pragma once
#include "Core/Resource.hpp"
#include "Libs/Serialization/BinarySerializer.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

namespace Engine
{

class AssetMeta
{};

class Asset
{
public:
    const AssetTypeID& GetAsseTypeID();

    Asset(std::unique_ptr<Resource>&& resource, const std::filesystem::path& path)
        : resource(std::move(resource)), path(path)
    {}

    Asset(const std::filesystem::path& path) : path(path) {}

    template <class T>
    void Serialize(Serializer* ser)
    {
        ser->Serialize("asset_assetTypeID", AssetFactory<T>::assetTypeID);
        resource->Serialize(ser);
    }

    bool Deserialize(Serializer* ser)
    {
        AssetTypeID assetTypeID;
        ser->Deserialize("asset_assetTypeID", assetTypeID);

        resource = AssetRegister::CreateAsset(assetTypeID);
        if (resource == nullptr)
        {
            return false;
        }
        resource->Deserialize(ser);

        return true;
    }

    Resource* GetResource() { return resource.get(); }
    const std::filesystem::path& GetPath() { return path; }

private:
    std::function<void(BinarySerializer* s)> serialize;
    std::function<void(BinarySerializer* s)> deserialize;

    std::unique_ptr<Resource> resource;
    std::filesystem::path path;
};
} // namespace Engine
