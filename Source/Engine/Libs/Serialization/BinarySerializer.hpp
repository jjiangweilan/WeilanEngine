#pragma once
#include "Core/Ptr.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include "Serializer.hpp"

class BinarySerializer : public Serializer
{
public:
    BinarySerializer(const DynamicArray<uint8_t>& data, SerializeReferenceResolveMap* resolve)
        : Serializer(data, resolve)
    {}

    BinarySerializer() {}

    // void Serialize(std::string_view name, const std::string& val) override;
    // void Deserialize(std::string_view name, std::string& val) override;
    //
    // void Serialize(std::string_view name, const UUID& uuid) override;
    // void Deserialize(std::string_view name, UUID& uuid) override;

    // DynamicArray<unsigned char> GetBinary() override
    // {
    //     return bytes;
    // }

private:
    size_t readOffset = 0;
    DynamicArray<unsigned char> bytes;
    // void Serialize(std::string_view name, unsigned char* v, size_t size) override;
    // void Deserialize(std::string_view name, unsigned char* v, size_t size) override;
};
