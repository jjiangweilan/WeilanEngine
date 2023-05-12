#pragma once
#include <string>
#include <unordered_map>
namespace Engine
{
#define GENERATE_SERIALIZABLE_FILE_ID(x) x
class Serializer;
template <class T>
struct SerializableField
{
    static void* Serialize(T* v, Serializer* s) { return nullptr; }
    static void* Deserialize(T* v, Serializer* s) { return nullptr; }
};

} // namespace Engine
