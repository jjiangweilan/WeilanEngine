#pragma once
#include "Libs/Ptr.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include "Serializer.hpp"

namespace Engine
{

class BinarySerializer : public Serializer
{
public:
    BinarySerializer(std::vector<unsigned char>&& bytes, SerializeReferenceResolveMap* referenceResolveMap)
        : bytes(std::move(bytes)), Serializer(referenceResolveMap)
    {}
    BinarySerializer(SerializeReferenceResolveMap* referenceResolveMap) : Serializer(referenceResolveMap) {}

    void Serialize(std::string_view name, const std::string& val) override;
    void Deserialize(std::string_view name, std::string& val) override;

    void Serialize(std::string_view name, const UUID& uuid) override;
    void Deserialize(std::string_view name, UUID& uuid) override;

    std::vector<unsigned char> GetBinary() override { return bytes; }

private:
    size_t readOffset = 0;
    std::vector<unsigned char> bytes;
    void Serialize(std::string_view name, unsigned char* v, size_t size) override;
    void Deserialize(std::string_view name, unsigned char* v, size_t size) override;
};
} // namespace Engine
