#pragma once
#include "Core/Resource.hpp"
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

    template<class T>
    void Save()
    {
        Serializer ser;
        ser.Serialize(AssetFactory<T>::assetTypeID);
        resource->Serialize(&ser);

        std::ofstream out;
        out.open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
        if (out.is_open() && out.good())
        {
            size_t size = 0;
            auto binary = ser.GetBinary(size);

            if (size != 0)
            {
                out.write((char*)binary, size);
            }
        }
        out.close();
    }

    bool Load()
    {
        std::ifstream in;
        in.open(path, std::ios_base::in | std::ios_base::binary);
        if (in.is_open() && in.good())
        {
            size_t size = std::filesystem::file_size(path);
            if (size != 0)
            {
                std::vector<unsigned char> bytes(size);
                in.read((char*)&bytes[0], size);

                Serializer ser(std::move(bytes));
                AssetTypeID assetTypeID;
                ser.Deserialize(assetTypeID);

                resource = AssetRegister::CreateAsset(assetTypeID);
                if (resource == nullptr)
                {
                    return false;
                }
            }
        }
        in.close();

        return true;
    }

    Resource* GetResource() { return resource.get(); }

private:
    std::function<void(Serializer* s)> serialize;
    std::function<void(Serializer* s)> deserialize;

    std::unique_ptr<Resource> resource;
    std::filesystem::path path;
};
} // namespace Engine
