#pragma once

#include "Libs/UUID.hpp"
#include <cinttypes>
#include <filesystem>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
namespace Engine
{
// fundamental type
template <class T>
size_t SerializerGetTypeSize(const T val)
{
    return sizeof(T);
}
template <class T>
void SerializerWriteToMem(void* ptr, const T& val, size_t size)
{
    memcpy(ptr, &val, size);
}
template <class T>
void SerializerReadFromMem(void* ptr, T& val, size_t size)
{
    memcpy(&val, ptr, size);
}

// string
template <>
inline size_t SerializerGetTypeSize(const std::string& val)
{
    return val.size() + 1;
}
template <>
inline void SerializerWriteToMem(void* ptr, const std::string& val, size_t size)
{
    memcpy(ptr, val.c_str(), size);
}
template <>
inline void SerializerReadFromMem(void* ptr, std::string& val, size_t size)
{
    val = std::string((char*)ptr);
}

// UUID
template <>
inline size_t SerializerGetTypeSize(const UUID& uuid)
{
    return uuid.ToString().size() + 1;
}
template <>
inline void SerializerWriteToMem(void* ptr, const UUID& uuid, size_t size)
{
    memcpy(ptr, uuid.ToString().c_str(), size);
}
template <>
inline void SerializerReadFromMem(void* ptr, UUID& uuid, size_t size)
{
    uuid = UUID(std::string((char*)ptr));
}

struct AssetSerializer
{
public:
    AssetSerializer();
    ~AssetSerializer();

    template <class T>
    void Write(const std::string& name, const T& val)
    {
        size_t size = SerializerGetTypeSize(val);
        size_t newSize = currentSize + size;
        GrowSizeIfNeeded(newSize);

        dataOffset[name] = currentSize;
        SerializerWriteToMem(mem + currentSize, val, size);
        currentSize = newSize;
    }

    template <class T>
    void Read(const std::string& name, T& val)
    {
        auto iter = dataOffset.find(name);
        if (iter != dataOffset.end())
        {
            Offset offset = iter->second;
            SerializerReadFromMem(mem + offset, val, SerializerGetTypeSize(val));
        }
    }

    void WriteToDisk(const std::filesystem::path& path);
    bool LoadFromDisk(const std::filesystem::path& path);

protected:
    using Offset = size_t;
    std::unordered_map<std::string, Offset> dataOffset;
    unsigned char* mem;
    size_t capacity;

private:
    void GrowSizeIfNeeded(size_t expected);
    size_t currentSize;
    const size_t size256Kb = 1024 * 256;
};
} // namespace Engine
