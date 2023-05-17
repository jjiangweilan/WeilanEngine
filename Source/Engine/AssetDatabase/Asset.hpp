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
    void Save(Serializer* ser)
    {
        ser->Serialize("asset_assetTypeID", AssetFactory<T>::assetTypeID);
        resource->Serialize(ser);

        std::ofstream out;
        out.open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
        if (out.is_open() && out.good())
        {
            auto binary = ser->GetBinary();

            if (binary.size() != 0)
            {
                out.write((char*)binary.data(), binary.size());
            }
        }
        out.close();
    }

    bool Load(Serializer* ser)
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
