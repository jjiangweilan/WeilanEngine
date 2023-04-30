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

template <class T, class U>
void Serializer::Serialize(const std::unordered_map<T, U>& val)
{
    uint32_t size = val.size();
    Serialize(size);
    for (auto& iter : val)
    {
        Serialize(iter.first);
        Serialize(iter.second);
    }
}
template <class T, class U>
void Serializer::Deserialize(std::unordered_map<T, U>& val)
{
    uint32_t size;
    Deserialize(size);
    for (int i = 0; i < size; ++i)
    {
        T key;
        U value;
        Deserialize(key);
        Deserialize(value);
        val[key] = value;
    }
}

template <class T>
void Serializer::Serialize(const UniPtr<T> val)
{
    Serialize(*val);
}

template <class T>
void Serializer::Deserialize(UniPtr<T>& val)
{
    Deserialize(*val);
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
