#include "BinarySerializer.hpp"

namespace Engine
{
void BinarySerializer::Serialize(std::string_view name, const std::string& val)
{
    uint32_t size = val.size();
    Serializer::Serialize(name, size);
    size_t offset = bytes.size();
    bytes.resize(offset + size);
    memcpy(&bytes[offset], &val.front(), size);
}

void BinarySerializer::Deserialize(std::string_view name, std::string& val)
{
    uint32_t size;
    Serializer::Deserialize(name, size);
    val = std::string(reinterpret_cast<char*>(&bytes[readOffset]), size);
    readOffset += size;
}

void BinarySerializer::Serialize(std::string_view name, const UUID& uuid)
{
    Serialize(name, uuid.ToString());
}

void BinarySerializer::Deserialize(std::string_view name, UUID& uuid)
{
    std::string s;
    Deserialize(name, s);
    uuid = s;
}

void BinarySerializer::Serialize(std::string_view name, unsigned char* v, size_t size)
{
    size_t offset = bytes.size();
    bytes.resize(offset + size);
    memcpy(&bytes[offset], v, size);
}

void BinarySerializer::Deserialize(std::string_view name, unsigned char* v, size_t size)
{
    size_t offset = bytes.size();
    bytes.resize(offset + size);
    memcpy(&bytes[offset], v, size);
}
} // namespace Engine
