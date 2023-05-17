#pragma once
#include "Libs/Ptr.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include "Serializer.hpp"
#include <nlohmann/json.hpp>

namespace Engine
{

class JsonSerializer : public Serializer
{
public:
    JsonSerializer(const nlohmann::json& j, SerializeReferenceResolveMap* referenceResolveMap = nullptr)
        : Serializer(referenceResolveMap), j(j)
    {}

    JsonSerializer() : Serializer(nullptr) {}

    JsonSerializer(SerializeReferenceResolveMap* referenceResolveMap) : Serializer(referenceResolveMap) {}

    void Serialize(std::string_view name, const std::string& val) override;
    void Deserialize(std::string_view name, std::string& val) override;

    void Serialize(std::string_view name, const UUID& uuid) override;
    void Deserialize(std::string_view name, UUID& uuid) override;

    std::vector<unsigned char> GetBinary() override
    {
        std::string b = j.dump();
        std::vector<unsigned char> a(b.begin(), b.end());
        return a;
    }

protected:
    void Serialize(std::string_view name, unsigned char* p, size_t size) override;
    void Deserialize(std::string_view name, unsigned char* p, size_t size) override;

private:
    nlohmann::json j;
};
} // namespace Engine
