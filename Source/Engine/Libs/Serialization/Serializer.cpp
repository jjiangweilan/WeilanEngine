#include "Serializer.hpp"

namespace Engine
{
template <>
void Serializer::Serialize(const std::string& val)
{
    uint32_t size = val.size();
    Serialize(size);
    size_t offset = bytes.size();
    bytes.resize(offset + size);
    memcpy(&bytes[offset], &val.front(), size);
}

template <>
void Serializer::Deserialize(std::string& val)
{
    uint32_t size;
    Deserialize(size);
    val = std::string(reinterpret_cast<char*>(&bytes[readOffset]), size);
    readOffset += size;
}

template <>
void Serializer::Serialize(const UUID& uuid)
{
    Serialize(uuid.ToString());
}
template <>
void Serializer::Deserialize(UUID& uuid)
{
    std::string s;
    Deserialize(s);
    uuid = s;
}

// template <class T>
// void Serializer::Serialize(T val)
// {
//     UUID uuid = val->GetUUID();
// }
// template <class T>
// void Serializer::Deserialize(T& val)
// {}
} // namespace Engine
