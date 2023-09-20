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
    JsonSerializer(const std::vector<uint8_t>& data, SerializeReferenceResolveMap* resolve) : Serializer(data, resolve)
    {
        j = nlohmann::json::parse(data.begin(), data.end());
    }

    JsonSerializer() : j(nlohmann::json::object()) {}

    void Serialize(std::string_view name, const std::string& val) override;
    void Deserialize(std::string_view name, std::string& val) override;

    void Serialize(std::string_view name, const UUID& uuid) override;
    void Deserialize(std::string_view name, UUID& uuid) override;

    void Serialize(std::string_view name, const uint32_t& v) override;
    void Deserialize(std::string_view name, uint32_t& v) override;

    void Serialize(std::string_view name, const int32_t& v) override;
    void Deserialize(std::string_view name, int32_t& v) override;

    void Serialize(std::string_view name, const float& v) override;
    void Deserialize(std::string_view name, float& v) override;

    void Serialize(std::string_view name, const glm::mat4& v) override;
    void Deserialize(std::string_view name, glm::mat4& v) override;

    void Serialize(std::string_view name, const glm::quat& v) override;
    void Deserialize(std::string_view name, glm::quat& v) override;

    void Serialize(std::string_view name, const glm::vec4& v) override;
    void Deserialize(std::string_view name, glm::vec4& v) override;

    void Serialize(std::string_view name, const glm::vec3& v) override;
    void Deserialize(std::string_view name, glm::vec3& v) override;

    void Serialize(std::string_view name, nullptr_t) override;
    bool IsNull(std::string_view name) override;

    std::vector<uint8_t> GetBinary() override
    {
        std::string b = j.dump();
        std::vector<uint8_t> a(b.begin(), b.end());
        return a;
    }

protected:
    void Serialize(std::string_view name, unsigned char* p, size_t size) override;
    void Deserialize(std::string_view name, unsigned char* p, size_t size) override;

    std::unique_ptr<Serializer> CreateSubserializer() override;
    void AppendSubserializer(std::string_view name, Serializer* s) override;
    std::unique_ptr<Serializer> CreateSubdeserializer(std::string_view name) override;

private:
    nlohmann::json j;
};
} // namespace Engine
