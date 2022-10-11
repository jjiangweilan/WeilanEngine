#include "AssetSerializer.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
namespace Engine
{
    AssetSerializer::AssetSerializer()
    {
        mem = new unsigned char[size256Kb];
        capacity = size256Kb;
        currentSize = 0;
    }

    AssetSerializer::~AssetSerializer()
    {
        delete []mem;
    }

    void AssetSerializer::GrowSizeIfNeeded(size_t expected)
    {
        while (capacity < expected)
        {
            capacity += size256Kb;
        }

        unsigned char* temp = mem;
        mem = new unsigned char[capacity];
        memcpy(mem, temp, currentSize);
        delete []temp;
    }

    void AssetSerializer::WriteToDisk(const std::filesystem::path& path)
    {
        nlohmann::json j;
        for(auto& iter : dataOffset)
        {
            j[iter.first] = iter.second;
        }
        std::filesystem::path p = path;
        p = std::filesystem::absolute(p);
        std::string jsonStr = j.dump();
        std::ofstream output(path, std::ios::binary | std::ios::trunc);

        if (output.good() && output.is_open())
        {
            output.write((char*)&currentSize, sizeof(uint64_t));
            uint64_t strSize = jsonStr.size() + 1;
            output.write((char*)&strSize, sizeof(uint64_t));
            output.write((char*)jsonStr.c_str(), strSize);
            output.write((const char*)mem, currentSize);
        }
        else
        {
            SPDLOG_ERROR("Failed to write to disk: {}", path.string());
        }
    }

    bool AssetSerializer::LoadFromDisk(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::in);
        if (!input.is_open() || !input.good()) return false;

        uint64_t dataSize = 0;
        uint64_t jsonStrSize = 0;
        input.read((char*)&dataSize, sizeof(uint64_t));
        input.read((char*)&jsonStrSize, sizeof(uint64_t));

        char* jsonStr = new char[jsonStrSize];
        input.read(jsonStr, jsonStrSize);

        mem = new unsigned char[dataSize];
        currentSize = dataSize;
        capacity = dataSize;
        input.read((char*)mem, dataSize);

        nlohmann::json j = nlohmann::json::parse(jsonStr);
        for(auto iter = j.begin(); iter != j.end(); ++iter)
        {
            dataOffset[iter.key()] = iter.value();
        }

        delete []jsonStr;
        return true;
    }
}